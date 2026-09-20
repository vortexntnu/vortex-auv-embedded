#!/usr/bin/env python3
"""
CAN simulator with selectable mode:

Modes:
- watchdog  : tests one watchdog node at a time
- alarms    : controls only the normal subsystem alarms
- thrusters : tests multi-thruster fault bitmaps while keeping watchdog alive

Target:
- Canable V2 on Windows
- python-can over slcan

# Run examples:
# py watchdog_can_simulator_V2.py --mode watchdog --channel COM4@115200 --bitrate 500000
# py watchdog_can_simulator_V2.py --mode alarms --channel COM4@115200 --bitrate 500000
# py watchdog_can_simulator_V2.py --mode thrusters --channel COM4@115200 --bitrate 500000
"""

from __future__ import annotations

import argparse
import sys
import threading
import time
from dataclasses import dataclass
from typing import Dict, List, Optional

import can


# ============================================================
# CAN IDs
# ============================================================

CAN_ID_PI = 0x101
CAN_ID_ORIN = 0x102
CAN_ID_MCU_POWER = 0x103
CAN_ID_KILLSWITCH = 0x104
CAN_ID_PRESSURE = 0x105
CAN_ID_ETHERNET = 0x106
CAN_ID_TEMPERATURE = 0x107
CAN_ID_THRUSTERS = 0x108
CAN_ID_SENSORS = 0x109
CAN_ID_POWER_CONSUMPTION = 0x10A
CAN_ID_SOFTWARE_MODE = 0x10B

CAN_ID_MON_GRIPPER = 0x300
CAN_ID_MON_BMS = 0x301
CAN_ID_MON_THRUSTER = 0x302
CAN_ID_MON_ACOUSTICS = 0x303
CAN_ID_MON_PI = 0x304
CAN_ID_MON_ORIN = 0x305

CAN_ID_ALIVE_REQ = 0x120

MSG_CLEAR = 0x00
MSG_WARN = 0x01
MSG_FAULT = 0x02

WIFI_NOT_CONNECTED = 0x10
WIFI_CONNECTED = 0x11

ALIVE_REQ_TYPE = 0xA1

DEFAULT_HEARTBEAT_PAYLOAD = [0x55]


# ============================================================
# Data model
# ============================================================

@dataclass
class SimNode:
    name: str
    short_name: str
    probe_target: int
    monitor_can_id: int
    response_can_id: int
    response_detail_value: int
    passive_interval_ms: int = 500
    passive_offset_ms: int = 0

    traffic_enabled: bool = True
    respond_enabled: bool = True
    next_tx_time: float = 0.0

    failure_active: bool = False
    recovery_scheduled: bool = False
    recovery_due_time: float = 0.0

    def passive_payload(self) -> List[int]:
        return DEFAULT_HEARTBEAT_PAYLOAD.copy()

    def response_payload(self) -> List[int]:
        return [MSG_CLEAR, self.response_detail_value]


# ============================================================
# Simulator
# ============================================================

class CanSimulator:
    def __init__(self, bus: can.BusABC, mode: str, verbose: bool = True) -> None:
        self.bus = bus
        self.mode = mode
        self.verbose = verbose
        self.stop_event = threading.Event()
        self.print_lock = threading.Lock()

        self.probe_response_delay_ms = 50
        self.startup_actions_delay_ms = 300

        # watchdog mode
        self.watchdog_fail_period_s = 10.0
        self.watchdog_recovery_delay_s = 5.0
        self.next_watchdog_fail_time = 0.0
        self.watchdog_fail_order = [
            "gripper",
            "bms",
            "thruster",
            "acoustics",
            "pi",
            "orin",
        ]
        self.watchdog_fail_index = 0

        # alarms mode
        self.normal_alarm_toggle_period_s = 30.0
        self.normal_alarms_active = False
        self.next_normal_toggle_time = 0.0

        # thrusters mode
        self.thruster_test_period_s = 10.0
        self.next_thruster_test_time = 0.0
        self.thruster_test_count = 0
        self.thruster_pattern_index = 0
        self.thruster_patterns = self._build_thruster_patterns()

        self.nodes: Dict[str, SimNode] = self._build_nodes()
        self.nodes_by_target: Dict[int, SimNode] = {
            node.probe_target: node for node in self.nodes.values()
        }

        self.rx_thread: Optional[threading.Thread] = None
        self.tx_thread: Optional[threading.Thread] = None
        self.startup_thread: Optional[threading.Thread] = None
        self.event_thread: Optional[threading.Thread] = None

    def _build_nodes(self) -> Dict[str, SimNode]:
        nodes = [
            SimNode(
                name="Gripper MCU",
                short_name="gripper",
                probe_target=1,
                monitor_can_id=CAN_ID_MON_GRIPPER,
                response_can_id=CAN_ID_MCU_POWER,
                response_detail_value=1,
                passive_offset_ms=0,
            ),
            SimNode(
                name="BMS MCU",
                short_name="bms",
                probe_target=2,
                monitor_can_id=CAN_ID_MON_BMS,
                response_can_id=CAN_ID_MCU_POWER,
                response_detail_value=2,
                passive_offset_ms=80,
            ),
            SimNode(
                name="Thruster MCU",
                short_name="thruster",
                probe_target=3,
                monitor_can_id=CAN_ID_MON_THRUSTER,
                response_can_id=CAN_ID_MCU_POWER,
                response_detail_value=3,
                passive_offset_ms=160,
            ),
            SimNode(
                name="Acoustics MCU",
                short_name="acoustics",
                probe_target=4,
                monitor_can_id=CAN_ID_MON_ACOUSTICS,
                response_can_id=CAN_ID_MCU_POWER,
                response_detail_value=4,
                passive_offset_ms=240,
            ),
            SimNode(
                name="Raspberry Pi",
                short_name="pi",
                probe_target=5,
                monitor_can_id=CAN_ID_MON_PI,
                response_can_id=CAN_ID_PI,
                response_detail_value=0,
                passive_offset_ms=320,
            ),
            SimNode(
                name="Orin",
                short_name="orin",
                probe_target=6,
                monitor_can_id=CAN_ID_MON_ORIN,
                response_can_id=CAN_ID_ORIN,
                response_detail_value=0,
                passive_offset_ms=400,
            ),
        ]
        return {node.short_name: node for node in nodes}

    def _build_thruster_patterns(self) -> List[int]:
        # Bit 0 = thruster 1, bit 7 = thruster 8
        # Deliberately varied patterns so you can verify sequencing and binary display.
        return [
            0b00000011,  # 1,2
            0b00010100,  # 3,5
            0b01000010,  # 2,7
            0b10001001,  # 1,4,8
            0b00111000,  # 4,5,6
            0b11000000,  # 7,8
            0b01010101,  # 1,3,5,7
            0b10101010,  # 2,4,6,8
            0b11110000,  # 5,6,7,8
            0b00001111,  # 1,2,3,4
            0b10010010,  # 2,5,8
            0b01100100,  # 3,6,7
        ]

    def log(self, msg: str) -> None:
        with self.print_lock:
            now = time.strftime("%H:%M:%S")
            print(f"[{now}] {msg}")

    def send_frame(self, arbitration_id: int, data: List[int], desc: str = "") -> None:
        msg = can.Message(
            arbitration_id=arbitration_id,
            data=data,
            is_extended_id=False,
        )
        try:
            self.bus.send(msg)
            if self.verbose:
                suffix = f" ({desc})" if desc else ""
                self.log(
                    f"TX 0x{arbitration_id:03X} [{len(data)}] "
                    f"{' '.join(f'{b:02X}' for b in data)}{suffix}"
                )
        except can.CanError as e:
            self.log(f"ERROR sending 0x{arbitration_id:03X}: {e}")

    # --------------------------------------------------------
    # Basic send helpers
    # --------------------------------------------------------

    def send_passive_heartbeat(self, node: SimNode) -> None:
        self.send_frame(
            arbitration_id=node.monitor_can_id,
            data=node.passive_payload(),
            desc=f"{node.name} heartbeat",
        )

    def send_im_good(self, node: SimNode) -> None:
        self.send_frame(
            arbitration_id=node.response_can_id,
            data=node.response_payload(),
            desc=f"{node.name} IM_GOOD",
        )

    def send_wifi_connected(self, which: str) -> None:
        which = which.lower().strip()
        if which == "pi":
            self.send_frame(CAN_ID_PI, [WIFI_CONNECTED], "Pi WiFi connected")
        elif which == "orin":
            self.send_frame(CAN_ID_ORIN, [WIFI_CONNECTED], "Orin WiFi connected")

    def send_wifi_disconnected(self, which: str) -> None:
        which = which.lower().strip()
        if which == "pi":
            self.send_frame(CAN_ID_PI, [WIFI_NOT_CONNECTED], "Pi WiFi disconnected")
        elif which == "orin":
            self.send_frame(CAN_ID_ORIN, [WIFI_NOT_CONNECTED], "Orin WiFi disconnected")

    def send_subsystem_clear(self, can_id: int, name: str) -> None:
        self.send_frame(can_id, [MSG_CLEAR, 0x00], f"{name} clear")

    def send_subsystem_fault(self, can_id: int, name: str, detail: int = 0) -> None:
        self.send_frame(can_id, [MSG_FAULT, detail & 0x0F], f"{name} fault")

    # --------------------------------------------------------
    # Thruster helpers
    # --------------------------------------------------------

    def thruster_bitmap_to_list(self, bitmap: int) -> List[int]:
        result: List[int] = []
        for bit in range(8):
            if bitmap & (1 << bit):
                result.append(bit + 1)
        return result

    def send_thruster_fault_bitmap(self, bitmap: int) -> None:
        bitmap &= 0xFF
        active = self.thruster_bitmap_to_list(bitmap)
        self.send_frame(
            CAN_ID_THRUSTERS,
            [MSG_FAULT, bitmap],
            desc=f"Thrusters fault bitmap 0x{bitmap:02X}",
        )
        self.log(
            f"Thruster test -> FAULT bitmap=0x{bitmap:02X} active_thrusters={active}"
        )

    def send_thruster_clear(self) -> None:
        self.send_subsystem_clear(CAN_ID_THRUSTERS, "Thrusters")
        self.log("Thruster test -> CLEAR all thruster errors")

    def run_next_thruster_test_step(self) -> None:
        self.thruster_test_count += 1

        # Every 5th period: clear all thruster errors
        if (self.thruster_test_count % 5) == 0:
            self.send_thruster_clear()
            return

        bitmap = self.thruster_patterns[self.thruster_pattern_index]
        self.thruster_pattern_index = (self.thruster_pattern_index + 1) % len(self.thruster_patterns)
        self.send_thruster_fault_bitmap(bitmap)

    # --------------------------------------------------------
    # Normal alarms
    # --------------------------------------------------------

    def send_all_normal_subsystem_clears(self) -> None:
        self.send_subsystem_clear(CAN_ID_KILLSWITCH, "Killswitch")
        self.send_subsystem_clear(CAN_ID_PRESSURE, "Pressure")
        self.send_subsystem_clear(CAN_ID_ETHERNET, "Ethernet")
        self.send_subsystem_clear(CAN_ID_TEMPERATURE, "Temperature")
        self.send_subsystem_clear(CAN_ID_THRUSTERS, "Thrusters")
        self.send_subsystem_clear(CAN_ID_SENSORS, "Sensors")
        self.send_subsystem_clear(CAN_ID_POWER_CONSUMPTION, "Power consumption")
        self.send_subsystem_clear(CAN_ID_SOFTWARE_MODE, "Software mode")

    def send_all_normal_subsystem_faults(self) -> None:
        self.send_subsystem_fault(CAN_ID_KILLSWITCH, "Killswitch")
        self.send_subsystem_fault(CAN_ID_PRESSURE, "Pressure")
        self.send_subsystem_fault(CAN_ID_ETHERNET, "Ethernet")
        self.send_subsystem_fault(CAN_ID_TEMPERATURE, "Temperature")
        self.send_subsystem_fault(CAN_ID_THRUSTERS, "Thrusters")
        self.send_subsystem_fault(CAN_ID_SENSORS, "Sensors")
        self.send_subsystem_fault(CAN_ID_POWER_CONSUMPTION, "Power consumption")
        self.send_subsystem_fault(CAN_ID_SOFTWARE_MODE, "Software mode")

    def toggle_normal_alarms(self) -> None:
        self.normal_alarms_active = not self.normal_alarms_active
        if self.normal_alarms_active:
            self.log("Normal alarms -> FAULT")
            self.send_all_normal_subsystem_faults()
        else:
            self.log("Normal alarms -> CLEAR")
            self.send_all_normal_subsystem_clears()

    # --------------------------------------------------------
    # Watchdog failures
    # --------------------------------------------------------

    def fail_watchdog_node(self, node_name: str) -> None:
        node = self.nodes[node_name]
        node.failure_active = True
        node.recovery_scheduled = False
        node.traffic_enabled = False
        node.respond_enabled = False
        self.log(f"Watchdog fail injected -> {node.name}")

    def schedule_watchdog_recovery(self, node: SimNode) -> None:
        if node.recovery_scheduled:
            return
        node.recovery_scheduled = True
        node.recovery_due_time = time.monotonic() + self.watchdog_recovery_delay_s
        self.log(f"Recovery scheduled for {node.name} in {self.watchdog_recovery_delay_s:.1f}s")

    def recover_watchdog_node(self, node: SimNode) -> None:
        node.failure_active = False
        node.recovery_scheduled = False
        node.traffic_enabled = True
        node.respond_enabled = True
        node.next_tx_time = time.monotonic() + (node.passive_offset_ms / 1000.0)
        self.log(f"Recovering watchdog node -> {node.name}")
        self.send_im_good(node)

    def rotate_next_watchdog_failure(self) -> None:
        for node in self.nodes.values():
            if node.failure_active or node.recovery_scheduled:
                return
        node_name = self.watchdog_fail_order[self.watchdog_fail_index]
        self.watchdog_fail_index = (self.watchdog_fail_index + 1) % len(self.watchdog_fail_order)
        self.fail_watchdog_node(node_name)

    # --------------------------------------------------------
    # Startup
    # --------------------------------------------------------

    def startup_actions(self) -> None:
        time.sleep(self.startup_actions_delay_ms / 1000.0)
        if self.stop_event.is_set():
            return

        # Clear all normal subsystem alarms at startup
        self.send_all_normal_subsystem_clears()
        time.sleep(0.05)
        self.send_wifi_connected("pi")
        time.sleep(0.05)
        self.send_wifi_connected("orin")

        # Also send IM_GOOD once for all watchdog-tracked nodes to make startup state obvious
        time.sleep(0.05)
        for node in self.nodes.values():
            self.send_im_good(node)
            time.sleep(0.02)

        if self.mode == "watchdog":
            self.log("Startup complete: watchdog mode")
        elif self.mode == "alarms":
            self.log("Startup complete: alarms mode")
        else:
            self.log("Startup complete: thrusters mode")

    # --------------------------------------------------------
    # Probe handling
    # --------------------------------------------------------

    def handle_probe(self, msg: can.Message) -> None:
        if self.mode == "alarms":
            return
        if msg.arbitration_id != CAN_ID_ALIVE_REQ:
            return
        if len(msg.data) < 2:
            self.log("RX probe frame too short, ignored")
            return
        if msg.data[0] != ALIVE_REQ_TYPE:
            self.log(f"RX 0x120 with unexpected type 0x{msg.data[0]:02X}, ignored")
            return

        target = int(msg.data[1])
        seq = int(msg.data[2]) if len(msg.data) >= 3 else None
        node = self.nodes_by_target.get(target)
        if node is None:
            self.log(f"No simulated node for probe target {target}, ignoring")
            return

        self.log(
            f"RX probe for {node.name} "
            f"(target={target}, seq={seq if seq is not None else 'n/a'})"
        )

        if node.failure_active:
            self.schedule_watchdog_recovery(node)
            self.log(f"{node.name}: currently failed -> not replying now")
            return

        if not node.respond_enabled:
            self.log(f"{node.name}: response disabled -> not replying")
            return

        time.sleep(self.probe_response_delay_ms / 1000.0)
        self.send_im_good(node)

    # --------------------------------------------------------
    # Threads
    # --------------------------------------------------------

    def rx_loop(self) -> None:
        self.log("RX loop started")
        while not self.stop_event.is_set():
            try:
                msg = self.bus.recv(timeout=0.2)
            except can.CanError as e:
                self.log(f"RX error: {e}")
                continue

            if msg is None:
                continue

            if self.verbose:
                data_str = " ".join(f"{b:02X}" for b in msg.data)
                self.log(f"RX 0x{msg.arbitration_id:03X} [{msg.dlc}] {data_str}")

            self.handle_probe(msg)

        self.log("RX loop stopped")

    def tx_loop(self) -> None:
        self.log("TX loop started")
        while not self.stop_event.is_set():
            # Keep watchdog traffic alive in watchdog and thrusters modes
            if self.mode in ("watchdog", "thrusters"):
                now = time.monotonic()
                for node in self.nodes.values():
                    if not node.traffic_enabled:
                        continue
                    interval_s = node.passive_interval_ms / 1000.0
                    if now >= node.next_tx_time:
                        self.send_passive_heartbeat(node)
                        node.next_tx_time += interval_s
                        if node.next_tx_time < now:
                            node.next_tx_time = now + interval_s
            time.sleep(0.005)

        self.log("TX loop stopped")

    def event_loop(self) -> None:
        self.log("Event loop started")
        base = time.monotonic()

        if self.mode == "watchdog":
            self.next_watchdog_fail_time = base + self.watchdog_fail_period_s
        elif self.mode == "alarms":
            self.next_normal_toggle_time = base + self.normal_alarm_toggle_period_s
        else:
            self.next_thruster_test_time = base + self.thruster_test_period_s

        while not self.stop_event.is_set():
            now = time.monotonic()

            if self.mode == "watchdog":
                if now >= self.next_watchdog_fail_time:
                    self.rotate_next_watchdog_failure()
                    self.next_watchdog_fail_time += self.watchdog_fail_period_s

                for node in self.nodes.values():
                    if node.recovery_scheduled and now >= node.recovery_due_time:
                        self.recover_watchdog_node(node)

            elif self.mode == "alarms":
                if now >= self.next_normal_toggle_time:
                    self.toggle_normal_alarms()
                    self.next_normal_toggle_time += self.normal_alarm_toggle_period_s

            else:
                if now >= self.next_thruster_test_time:
                    self.run_next_thruster_test_step()
                    self.next_thruster_test_time += self.thruster_test_period_s

            time.sleep(0.05)

        self.log("Event loop stopped")

    def start(self) -> None:
        self.stop_event.clear()
        base = time.monotonic()
        for node in self.nodes.values():
            node.next_tx_time = base + (node.passive_offset_ms / 1000.0)

        self.rx_thread = threading.Thread(target=self.rx_loop, daemon=True)
        self.tx_thread = threading.Thread(target=self.tx_loop, daemon=True)
        self.startup_thread = threading.Thread(target=self.startup_actions, daemon=True)
        self.event_thread = threading.Thread(target=self.event_loop, daemon=True)

        self.rx_thread.start()
        self.tx_thread.start()
        self.startup_thread.start()
        self.event_thread.start()

        self.log(f"Simulator started in mode: {self.mode}")

    def stop(self) -> None:
        self.stop_event.set()
        for t in [self.rx_thread, self.tx_thread, self.startup_thread, self.event_thread]:
            if t is not None:
                t.join(timeout=1.0)
        self.log("Simulator stopped")

    # --------------------------------------------------------
    # Runtime controls
    # --------------------------------------------------------

    def list_status(self) -> None:
        with self.print_lock:
            print(f"\nMode: {self.mode}")
            if self.mode in ("watchdog", "thrusters"):
                print(
                    f"{'Node':<12} {'Traffic':<8} {'Respond':<8} "
                    f"{'Failed':<8} {'Recovering':<11} {'Target':<6}"
                )
                print("-" * 68)
                for node in self.nodes.values():
                    print(
                        f"{node.short_name:<12} "
                        f"{str(node.traffic_enabled):<8} "
                        f"{str(node.respond_enabled):<8} "
                        f"{str(node.failure_active):<8} "
                        f"{str(node.recovery_scheduled):<11} "
                        f"{node.probe_target:<6}"
                    )
                if self.mode == "thrusters":
                    print(f"\nThruster test count: {self.thruster_test_count}")
                    print(f"Next pattern index: {self.thruster_pattern_index}")
            else:
                print(f"Normal alarms active: {self.normal_alarms_active}")
            print()

    def get_node(self, node_name: str) -> Optional[SimNode]:
        return self.nodes.get(node_name.strip().lower())

    def print_help(self) -> None:
        with self.print_lock:
            if self.mode == "watchdog":
                print(
                    """
Mode: watchdog

Commands:
  help
  status
  nodes
  verbose on|off

  stop <node>
  start <node>
  stopall
  startall

  mute <node>
  unmute <node>
  muteall
  unmuteall

  good <node>
  fail <node>
  recover <node>

  quit / exit
"""
                )
            elif self.mode == "alarms":
                print(
                    """
Mode: alarms

Commands:
  help
  status
  verbose on|off

  clear_all
  fault_all
  toggle_normal

  wifi_good pi|orin
  wifi_bad pi|orin

  quit / exit
"""
                )
            else:
                print(
                    """
Mode: thrusters

Commands:
  help
  status
  nodes
  verbose on|off

  send <bitmap>
  clear_thrusters
  next_thruster

  stop <node>
  start <node>
  stopall
  startall

  mute <node>
  unmute <node>
  muteall
  unmuteall

  good <node>

  quit / exit
"""
                )

    def print_nodes(self) -> None:
        with self.print_lock:
            print("Nodes: gripper, bms, thruster, acoustics, pi, orin")

    def command_loop(self) -> None:
        self.print_help()
        while True:
            try:
                cmd = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break

            if not cmd:
                continue

            parts = cmd.split()
            head = parts[0].lower()

            if head in ("quit", "exit"):
                break
            elif head == "help":
                self.print_help()
            elif head == "status":
                self.list_status()
            elif head == "verbose":
                if len(parts) != 2 or parts[1].lower() not in ("on", "off"):
                    self.log("Usage: verbose on|off")
                    continue
                self.verbose = (parts[1].lower() == "on")
                self.log(f"Verbose set to {self.verbose}")

            elif self.mode == "watchdog":
                if head == "nodes":
                    self.print_nodes()
                elif head == "stopall":
                    for node in self.nodes.values():
                        node.traffic_enabled = False
                    self.log("All passive traffic DISABLED")
                elif head == "startall":
                    for node in self.nodes.values():
                        node.traffic_enabled = True
                    self.log("All passive traffic ENABLED")
                elif head == "muteall":
                    for node in self.nodes.values():
                        node.respond_enabled = False
                    self.log("All probe responses DISABLED")
                elif head == "unmuteall":
                    for node in self.nodes.values():
                        node.respond_enabled = True
                    self.log("All probe responses ENABLED")
                elif head in ("stop", "start", "mute", "unmute", "good", "fail", "recover"):
                    if len(parts) != 2:
                        self.log(f"Usage: {head} <node>")
                        continue
                    node = self.get_node(parts[1])
                    if node is None:
                        self.log(f"Unknown node: {parts[1]}")
                        continue

                    if head == "stop":
                        node.traffic_enabled = False
                        self.log(f"{node.name}: passive traffic DISABLED")
                    elif head == "start":
                        node.traffic_enabled = True
                        self.log(f"{node.name}: passive traffic ENABLED")
                    elif head == "mute":
                        node.respond_enabled = False
                        self.log(f"{node.name}: probe response DISABLED")
                    elif head == "unmute":
                        node.respond_enabled = True
                        self.log(f"{node.name}: probe response ENABLED")
                    elif head == "good":
                        self.send_im_good(node)
                    elif head == "fail":
                        self.fail_watchdog_node(node.short_name)
                    elif head == "recover":
                        self.recover_watchdog_node(node)
                else:
                    self.log(f"Unknown command: {cmd}")

            elif self.mode == "alarms":
                if head == "clear_all":
                    self.send_all_normal_subsystem_clears()
                    self.normal_alarms_active = False
                elif head == "fault_all":
                    self.send_all_normal_subsystem_faults()
                    self.normal_alarms_active = True
                elif head == "toggle_normal":
                    self.toggle_normal_alarms()
                elif head == "wifi_good" and len(parts) == 2:
                    self.send_wifi_connected(parts[1])
                elif head == "wifi_bad" and len(parts) == 2:
                    self.send_wifi_disconnected(parts[1])
                else:
                    self.log(f"Unknown command: {cmd}")

            else:
                if head == "nodes":
                    self.print_nodes()
                elif head == "clear_thrusters":
                    self.send_thruster_clear()
                elif head == "next_thruster":
                    self.run_next_thruster_test_step()
                elif head == "send":
                    if len(parts) != 2:
                        self.log("Usage: send <bitmap>")
                        continue
                    try:
                        bitmap = int(parts[1], 0)
                    except ValueError:
                        self.log("Bitmap must be an integer, e.g. 0x52 or 82")
                        continue
                    self.send_thruster_fault_bitmap(bitmap)
                elif head == "stopall":
                    for node in self.nodes.values():
                        node.traffic_enabled = False
                    self.log("All passive traffic DISABLED")
                elif head == "startall":
                    for node in self.nodes.values():
                        node.traffic_enabled = True
                    self.log("All passive traffic ENABLED")
                elif head == "muteall":
                    for node in self.nodes.values():
                        node.respond_enabled = False
                    self.log("All probe responses DISABLED")
                elif head == "unmuteall":
                    for node in self.nodes.values():
                        node.respond_enabled = True
                    self.log("All probe responses ENABLED")
                elif head in ("stop", "start", "mute", "unmute", "good"):
                    if len(parts) != 2:
                        self.log(f"Usage: {head} <node>")
                        continue
                    node = self.get_node(parts[1])
                    if node is None:
                        self.log(f"Unknown node: {parts[1]}")
                        continue

                    if head == "stop":
                        node.traffic_enabled = False
                        self.log(f"{node.name}: passive traffic DISABLED")
                    elif head == "start":
                        node.traffic_enabled = True
                        self.log(f"{node.name}: passive traffic ENABLED")
                    elif head == "mute":
                        node.respond_enabled = False
                        self.log(f"{node.name}: probe response DISABLED")
                    elif head == "unmute":
                        node.respond_enabled = True
                        self.log(f"{node.name}: probe response ENABLED")
                    elif head == "good":
                        self.send_im_good(node)
                else:
                    self.log(f"Unknown command: {cmd}")

        self.log("Leaving command loop")


# ============================================================
# Main
# ============================================================

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="CAN simulator")
    parser.add_argument(
        "--mode",
        required=True,
        choices=["watchdog", "alarms", "thrusters"],
        help="Simulator mode",
    )
    parser.add_argument(
        "--channel",
        default="COM4@115200",
        help="slcan channel, e.g. COM4@115200",
    )
    parser.add_argument(
        "--bitrate",
        type=int,
        default=500000,
        help="CAN bitrate in bit/s",
    )
    parser.add_argument(
        "--passive-ms",
        type=int,
        default=500,
        help="Heartbeat period in ms (watchdog/thrusters mode only)",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Reduce TX/RX printout",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        bus = can.Bus(
            interface="slcan",
            channel=args.channel,
            bitrate=args.bitrate,
        )
    except Exception as e:
        print(f"Failed to open CAN bus: {e}", file=sys.stderr)
        return 1

    sim = CanSimulator(bus=bus, mode=args.mode, verbose=not args.quiet)

    for node in sim.nodes.values():
        node.passive_interval_ms = args.passive_ms

    try:
        sim.start()
        sim.command_loop()
    finally:
        sim.stop()
        try:
            bus.shutdown()
        except Exception:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())