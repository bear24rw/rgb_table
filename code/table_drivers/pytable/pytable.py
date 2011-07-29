import sys
import numpy
import serial

class Table():

    def __init__(self, width=16, height=8, device='/dev/ttyUSB0', baud=500000):

        self.WIDTH = width
        self.HEIGHT = height

        # initialize the table
        self.data = numpy.zeros(width*height*3).reshape(width, height, 3) 

        # initialize serial port
        self.s = serial.Serial(device, baud)

    def get(self, x, y):

        r = self.data[x][y][0]
        g = self.data[x][y][1]
        b = self.data[x][y][2]

        return [r, g, b]

    def set(self, x, y, color):

        self.data[x][y] = color

    def set_all(self, color):

        for x in range(self.WIDTH):
            for y in range(self.HEIGHT):
                self.data[x][y] = color

    def send(self):

        self.s.write(chr(0xFF))

        for y in range(self.HEIGHT):
            for x in range(self.WIDTH):
                
                if (y >= self.HEIGHT / 2):
                    self.s.write(chr(int(self.data[x][y][0])))
                    self.s.write(chr(int(self.data[x][y][1])))
                    self.s.write(chr(int(self.data[x][y][2])))
                else:
                    self.s.write(chr(int(self.data[self.WIDTH-x-1][y][0])))
                    self.s.write(chr(int(self.data[self.WIDTH-x-1][y][1])))
                    self.s.write(chr(int(self.data[self.WIDTH-x-1][y][2])))




table = Table()
table.set_all([0,0,254])
table.send()
