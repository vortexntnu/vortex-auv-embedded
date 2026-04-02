import can
import struct

CAN_ID = 0x200
CHANNEL = 1#"COM9"  # Change to your actual COM port

def parse_message(data: bytes):
    if len(data) != 16:
        print(f"Unexpected data length: {len(data)}")
        return
    floats = struct.unpack("<4f", data)  # Little-endian, 4x float32
    print(f"  f1={floats[0]:.4f}  f2={floats[1]:.4f}  f3={floats[2]:.4f}  f4={floats[3]:.4f}")

with can.Bus(
    interface="gs_usb",
    channel=CHANNEL,
    bitrate=500000,
    data_bitrate=2000000,
    fd=True,
) as bus:
    print(f"Listening for CAN FD ID 0x{CAN_ID:X}...")
    for msg in bus:
        if msg.arbitration_id == CAN_ID:
            print(f"[{msg.timestamp:.3f}] ID=0x{msg.arbitration_id:X} FD={msg.is_fd}")
            parse_message(bytes(msg.data))