#!/usr/bin/python2.7

class thinker():
    def __init__(self):
        self.a = 10
        self.b = 11
        self.myfunc = lambda x,a=self.a,b=self.b: (x+a+b)

    def update(self):
        self.a = 0
        self.b = 0

    def printme(self):
        print self.myfunc(6)

mm = thinker()
mm.printme()
mm.update()
mm.printme()
