from serialutil import GcodeSender
from pointutil import inverseKinematics, interpolatePoints, convertWaypoints
from operator import itemgetter
import math
import time
def square():

    a = inverseKinematics(100, 200)
    b = inverseKinematics(-100, 200)
    c = inverseKinematics(-100, 300)
    d = inverseKinematics(100, 300)
    e = inverseKinematics(100, 200)
    sender.sendangle(*a)
    time.sleep(5)
    sender.sendangle(*b)
    time.sleep(5)
    sender.sendangle(*c)
    time.sleep(5)
    sender.sendangle(*d)
    time.sleep(5)
    sender.sendangle(*e)

def circle():
    a = inverseKinematics(100, 200)
    b = inverseKinematics(90, 180)
    c = inverseKinematics(80, 160)
    d = inverseKinematics(90, 140)
    e = inverseKinematics(100, 120)
    f = inverseKinematics(110, 140)
    g = inverseKinematics(120, 160)
    h = inverseKinematics(110, 180)
    i = inverseKinematics(100, 200)
    sender.sendangle(*a)
    time.sleep(5)
    sender.sendangle(*b)
    time.sleep(5)
    sender.sendangle(*c)
    time.sleep(5)
    sender.sendangle(*d)
    time.sleep(5)
    sender.sendangle(*e)
    time.sleep(5)
    sender.sendangle(*f)
    time.sleep(5)
    sender.sendangle(*g)
    time.sleep(5)
    sender.sendangle(*h)
    time.sleep(5)
    sender.sendangle(*i)
    time.sleep(5)

def star():
    theta = [10, 20, 30, 40, 50, 60, 70, 80]
    for n in theta:
        x = 10/360 * math.cos(n * math.pi/180)
        y = 10/360 * math.sin(n * math.pi/180)
        a = inverseKinematics(x, y)
        sender.sendangle(*a)
        time.sleep(5)




if __name__ == "__main__":
    sender = GcodeSender()
    print(sender.serialConnection)
    sender.setup()
    sender.initializePosition(-45, -130)
    star()

    #points = square()
    #for n in points:
        #sender.sendangle(*n)
        #time.sleep(2)
