import pygame
import colorsys
import time
import sys
import math
import random as rand
import noise
from pytable import Table

WIDTH = 16
HEIGHT = 8
CELL_SIZE = 20


pygame.display.init()
screen = pygame.display.set_mode((WIDTH*CELL_SIZE,HEIGHT*CELL_SIZE))
s = pygame.Surface((WIDTH*CELL_SIZE,HEIGHT*CELL_SIZE))
s.fill((0,0,0))
clock = pygame.time.Clock()

table = Table()

grid = [[[0,0,0] for i in range(HEIGHT)] for j in range(WIDTH)]

def clear_grid():
    global grid
    grid = [[[0,0,0] for i in range(HEIGHT)] for j in range(WIDTH)]

def hsv_to_rgb(x):
    y = colorsys.hsv_to_rgb(x[0], x[1], x[2])
    return [y[0]*254, y[1]*254, y[2]*254]
    
def rgb_to_hsv(x):
    y = colorsys.rgb_to_hsv(x[0], x[1], x[2])
    return [y[0], y[1], y[2]]
    
def draw_cells():
    for x in range(WIDTH):
        for y in range(HEIGHT):
            color = hsv_to_rgb(grid[x][y])
            pygame.draw.rect(s, color, pygame.Rect(x*CELL_SIZE,y*CELL_SIZE,CELL_SIZE,CELL_SIZE), 0)
            
    for i in range(WIDTH):
        pygame.draw.line(s, (255,255,255), (i*CELL_SIZE, 0), (i*CELL_SIZE, HEIGHT*CELL_SIZE), 1)
    for i in range(HEIGHT):
        pygame.draw.line(s, (255,255,255), (0,i*CELL_SIZE), (WIDTH*CELL_SIZE,i*CELL_SIZE), 1)

        

z = 0
z2 = 0
scale = 8.0/64.0
octave = 1

mmax = 0.0001
mmin = 0.0001

mode = 1

def draw_dots():
    global z,z2,mmin,mmax
    clear_grid()

    for x in range(WIDTH):
        for y in range(HEIGHT):
            c = noise.pnoise3(x * scale, y * scale, z * scale, octave, repeatx=WIDTH, repeaty=HEIGHT)

            if c > mmax: mmax = c
            if c < mmin: mmin = c
        
            c = c + math.fabs(mmin)
            c = c / (mmax + math.fabs(mmin))
            if mode == 1:
                c = (c + 0.2) % 1
                grid[x][y] = [c,1,1]
            elif mode == 2:
                c = (c + z2) % 1
                grid[x][y] = [c,1,1]
            else:
                grid[x][y] = [z2%1,1,c]

    z += .1
    z2 += .005

done = False

while not done:
    clock.tick(30)

    for event in pygame.event.get():
        if event.type == pygame.QUIT: done = True
        if event.type == pygame.KEYUP:

            if event.key == pygame.K_a: octave += 1
            if event.key == pygame.K_s: octave -= 1
            if event.key == pygame.K_q: scale += 0.005
            if event.key == pygame.K_w: scale -= 0.005
            if event.key == pygame.K_SPACE:
                if mode == 1: mode = 2
                elif mode == 2: mode = 3
                elif mode == 3: mode = 1

            print "Octave: %d Scale: %f Min: %f Max: %f Mode: %d" % (octave, scale, mmin, mmax, mode)

    draw_dots()
    draw_cells()
    screen.fill((0,255,0))
    screen.blit(s,(0,0))
    pygame.display.flip()

    
    for x in range(WIDTH):
        for y in range(HEIGHT):
            
            table.data[x][y] = hsv_to_rgb(grid[x][y])

    table.send()
