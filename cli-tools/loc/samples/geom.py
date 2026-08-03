# Tiny geometry helpers -- a sample file so `loc` has something to count.
import math


def circle_area(r):
    """Area of a circle."""
    return math.pi * r * r


def rect_area(w, h):
    # width times height
    return w * h


class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y

    def dist(self, other):
        dx = self.x - other.x
        dy = self.y - other.y
        return math.sqrt(dx * dx + dy * dy)
