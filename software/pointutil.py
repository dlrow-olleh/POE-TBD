import math
import numpy as np


def inverseKinematics(x, y):
    D = math.sqrt(x ** 2 + y ** 2)
    if x > 0:
        angle1 = math.pi + math.atan(x / y) + math.acos(D / 400)
        angle2 = math.pi + math.atan(x / y) - math.acos(D / 400)
    else:
        angle1 = math.pi - math.atan(x / y) + math.acos(D / 400)
        angle2 = math.pi - math.atan(x / y) - math.acos(D / 400)

    return np.rad2deg(angle1), np.rad2deg(angle2)


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
