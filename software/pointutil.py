import math
import numpy as np


def inverseKinematics(x, y):

    x *= -1

    OFFSET = 0
    YAXIS = 0
    LENGTH = 200

    distance = math.sqrt((OFFSET - x) * (OFFSET - x) + (YAXIS - y) * (YAXIS - y))
    if (x > OFFSET):
        angle1 = math.pi + math.acos(distance / (2 * LENGTH)) - math.atan((x - OFFSET) / (YAXIS - y))
    else:
        angle1 = math.pi + math.acos(distance / (2 * LENGTH)) + math.atan((OFFSET - x) / (YAXIS - y))

    if (x > OFFSET):
        angle2 = math.pi - math.acos(distance / (2 * LENGTH)) - math.atan((x - OFFSET) / (YAXIS - y))
    else:
        angle2 = math.pi - math.acos(distance / (2 * LENGTH)) + math.atan((OFFSET - x) / (YAXIS - y))

    newangle1 = math.atan2(-math.sin(math.fmod((angle1 + (-math.pi / 2)), math.pi * 2)),
                           math.cos(math.fmod((angle1 + (-math.pi / 2)), math.pi * 2)))
    newangle2 = math.atan2(-math.sin(math.fmod((angle2 + (-math.pi / 2)), math.pi * 2)),
                           math.cos(math.fmod((angle2 + (-math.pi / 2)), math.pi * 2)))

    if newangle1 > math.pi:
        newangle1 -= 2*math.pi
    if newangle2 > math.pi:
        newangle2 -= 2 *math.pi

    return np.rad2deg(newangle1), np.rad2deg(newangle2)


def interpolatePoints(P1, P2, res=0.5):
    """
    :param P1: Start point (numpy array)
    :param P2: End point (numpy array)
    :param res: Max distance between points
    :return:
    """
    if type(P1) == list:
        P1 = np.asarray(P1)
    if type(P2) == list:
        P2 = np.asarray(P2)

    diff = P2 - P1
    dist = np.linalg.norm(diff)
    numpoints = dist / res
    points = np.linspace(P1, P2, numpoints + 1)
    return points


def convertWaypoints(p1, p2):
    interpPoints = interpolatePoints(p1, p2)
    waypoints = []
    for point in interpPoints:
        solvedpoint = inverseKinematics(*point)  # *point expands to point[0], point[1]
        waypoints.append(solvedpoint)
    return waypoints


if __name__ == "__main__":
    print(interpolatePoints(
        [0, 0], [10, 0], 1
    ))
    print(inverseKinematics(-200,-200))
    print(convertWaypoints([10,1], [-10,1]))
