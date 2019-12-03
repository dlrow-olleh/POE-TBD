from serialutil import GcodeSender
from pointutil import inverseKinematics, interpolatePoints, convertWaypoints

def square():
    points = []
    a = convertWaypoints([10, 10], [-10, 10])
    b = convertWaypoints([-10, 10], [-10, 1])
    c = convertWaypoints([-10, 1], [10, 1])
    d = convertWaypoints([10, 1], [10, 10])
    for point in a, b, c, d:
        points.append(point)
    for point in points:
        sender.sendangle(point)
if __name__ == "__main__":
    sender = GcodeSender()
    sender.setup()
    sender.initializePosition()
    square()
    time.sleep(1)
