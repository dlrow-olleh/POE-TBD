import time

import numpy
import serial


class GcodeSender:
    def __init__(self, port="/dev/ttyACM0", baud=115200):
        self.baud = baud
        self.port = port
        self.serialConnection = serial.Serial(self.port, self.baud, timeout=5)

    def __del__(self):
        self.serialConnection.close()

    def setup(self):
        self.writeGcode("G21") # set millimeters mode
        self.writeGcode("G90") # set absolute mode

    def initializePosition(self, x=0, y=0):
        initcmd = f"G10 P0 L20 X{x:.3f} Y{y:.3f} Z0"
        self.writeGcode(initcmd)

    def sendangle(self, x, y, feed=300):
        cmd = f"G1 X{x:.3f} Y{y:.3f} F{feed:.3f}"
        self.writeGcode(cmd)

    def writeGcode(self, gcodeLine):
        gcodeLine = (gcodeLine + "\n").encode("utf-8")
        self.serialConnection.write(gcodeLine)
        self.serialConnection.flush()


if __name__ == "__main__":
    sender = GcodeSender()
    sender.setup()
    sender.initializePosition()
    sender.sendangle(0, 90)
    time.sleep(1)
