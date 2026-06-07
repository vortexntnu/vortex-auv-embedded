import argparse
import importlib
import math
import time


TOPIC_NAME = "/nautilus/acoustics/bearing_measurement"
MSG_TYPE = "vortex_msgs.msg.BearingMeasurement"
PUBLISH_HZ = 20.0
FRAME_ID = "acoustics_frame"


def import_message_type(type_path: str):
	module_path, _, class_name = type_path.rpartition(".")
	if not module_path or not class_name:
		raise ValueError(f"Invalid message type '{type_path}'. Use '<pkg>.msg.<MessageClass>'.")

	module = importlib.import_module(module_path)
	return getattr(module, class_name)


def _set_nested_attr(obj, path, value):
	cur = obj
	for key in path[:-1]:
		if not hasattr(cur, key):
			return False
		cur = getattr(cur, key)

	leaf = path[-1]
	if not hasattr(cur, leaf):
		return False
	setattr(cur, leaf, value)
	return True


def _get_nested_attr(obj, path):
	cur = obj
	for key in path:
		if not hasattr(cur, key):
			return None
		cur = getattr(cur, key)
	return cur


def _bearing_vector(t_sec, target_id):
	# Simple deterministic motion so the point cloud sweeps around for visualization.
	az = 45.0 * math.sin(0.45 * t_sec + 0.9 * target_id)
	el = 25.0 * math.sin(0.24 * t_sec + 0.5 * target_id)
	azr = math.radians(az)
	elr = math.radians(el)

	x = math.cos(elr) * math.cos(azr)
	y = math.cos(elr) * math.sin(azr)
	z = math.sin(elr)
	return x, y, z


def _weight_signal(t_sec, target_id):
	w = 0.75 + 0.25 * math.sin(0.7 * t_sec + target_id)
	return max(0.0, min(1.0, w))


def fill_message(msg, clock, frame_id, t_sec, target_id):
	x, y, z = _bearing_vector(t_sec, target_id)
	weight = _weight_signal(t_sec, target_id)

	vector_ok = True
	vector_ok &= _set_nested_attr(msg, ("bearing", "vector", "x"), float(x))
	vector_ok &= _set_nested_attr(msg, ("bearing", "vector", "y"), float(y))
	vector_ok &= _set_nested_attr(msg, ("bearing", "vector", "z"), float(z))

	stamp = clock.now().to_msg()
	_set_nested_attr(msg, ("bearing", "header", "stamp"), stamp)
	_set_nested_attr(msg, ("bearing", "header", "frame_id"), frame_id)

	weight_ok = _set_nested_attr(msg, ("weight",), float(weight))
	target_ok = _set_nested_attr(msg, ("target_id",), int(target_id))

	if vector_ok and weight_ok and target_ok:
		return msg, x, y, z, weight, target_id

	# Fallback for std_msgs/Float32MultiArray or custom array-like messages.
	if hasattr(msg, "data"):
		msg.data = [float(x), float(y), float(z), float(weight), float(target_id)]
		return msg, x, y, z, weight, target_id

	raise TypeError(
		"Message type does not match expected fields. Expected '\n"
		"  bearing.vector.{x,y,z}, weight, target_id\n"
		"or at least a 'data' array fallback."
	)


def parse_args():
	parser = argparse.ArgumentParser(description="ROS2 bearing measurement emulator for live viewer testing")
	parser.add_argument("--topic", default=TOPIC_NAME, help="ROS2 topic to publish bearing messages")
	parser.add_argument("--msg-type", default=MSG_TYPE, help="Message type in form '<pkg>.msg.<MessageClass>'")
	parser.add_argument("--hz", type=float, default=PUBLISH_HZ, help="Publish rate in Hz")
	parser.add_argument("--frame-id", default=FRAME_ID, help="Frame ID for bearing.header.frame_id")
	parser.add_argument("--target-id", type=int, default=None, help="Publish only one target id")
	parser.add_argument("--targets", type=int, default=2, help="Number of targets if --target-id is not set")
	return parser.parse_args()


def main():
	args = parse_args()

	rclpy = importlib.import_module("rclpy")
	Node = importlib.import_module("rclpy.node").Node

	msg_cls = import_message_type(args.msg_type)
	rclpy.init(args=None)

	class BearingEmulator(Node):
		def __init__(self):
			super().__init__("ros2_acoustics_emulator")
			self.publisher = self.create_publisher(msg_cls, args.topic, 10)
			self.period = 1.0 / max(args.hz, 1e-3)
			self.start_time = time.time()
			self.counter = 0
			self.warned_fallback = False
			self.timer = self.create_timer(self.period, self.publish_once)

			self.get_logger().info(
				f"Publishing emulated bearings on '{args.topic}' ({args.msg_type}) at {args.hz:.2f} Hz"
			)

		def _target_for_tick(self):
			if args.target_id is not None:
				return int(args.target_id)

			targets = max(1, int(args.targets))
			return int(self.counter % targets)

		def publish_once(self):
			t_sec = time.time() - self.start_time
			target_id = self._target_for_tick()

			msg = msg_cls()
			try:
				msg, x, y, z, weight, target_id = fill_message(
					msg=msg,
					clock=self.get_clock(),
					frame_id=args.frame_id,
					t_sec=t_sec,
					target_id=target_id,
				)
			except Exception as exc:
				self.get_logger().error(f"Failed to fill message: {exc}")
				return

			self.publisher.publish(msg)
			self.counter += 1

			if hasattr(msg, "data") and not self.warned_fallback:
				self.get_logger().warn(
					"Using array fallback format [x, y, z, weight, target_id]. "
					"Switch msg type to your custom bearing message for full compatibility."
				)
				self.warned_fallback = True

			self.get_logger().info(
				f"pub target_id={target_id} vec=({x:.3f}, {y:.3f}, {z:.3f}) weight={weight:.3f}"
			)

	node = BearingEmulator()

	try:
		rclpy.spin(node)
	except KeyboardInterrupt:
		pass
	finally:
		node.destroy_node()
		rclpy.shutdown()


if __name__ == "__main__":
	main()
