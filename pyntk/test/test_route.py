# This file is part of Netsukuku
# (c) Copyright 2007 Daniele Tricoli aka Eriol <eriol@mornie.org>
#
# Tests for ntk.core.route


import sys
import unittest
sys.path.append('..')

from operator import add

from ntk.core.route import (NullRem, DeadRem, Rtt, Bw, Avg, RemError,
                             AvgSumError, RouteGw, RouteGwError, RouteNode)

class TestRouteEfficiencyMeasure(unittest.TestCase):

    def setUp(self):
        rem_value = 1
        self.null_rem = NullRem(rem_value)
        self.dead_rem = DeadRem(rem_value)
        self.rtt = Rtt(rem_value)
        self.bw = Bw(rem_value, 1, 1)
        self.avg = Avg([Rtt(1), Rtt(5), Rtt(10)])

    def testAdd2NullRem(self):
        ''' Sum 2 NullRem'''
        self.failUnless(NullRem(1) + self.null_rem is self.null_rem)

    def testAddNullRemDeadRem(self):
        ''' Sum a NullRem with a DeadRem '''
        self.failUnless(self.null_rem + self.dead_rem is self.dead_rem)

    def testAddDeadRemNullRem(self):
        ''' Sum a DeadRem with a NullRem '''
        self.failUnless(self.dead_rem + self.null_rem is self.dead_rem)

    def testAddRttNullRem(self):
        ''' Sum a Rtt with a NullRem '''
        new_rtt = self.rtt + self.null_rem

        self.failUnless(isinstance(new_rtt, Rtt))
        self.failUnlessEqual(new_rtt.value,
                             self.rtt.value + self.null_rem.value)

    def testAddRttDeadRem(self):
        ''' Sum a Rtt with a DeadRem '''
        self.failUnless(self.rtt + self.dead_rem is self.dead_rem)

    def testCompareRtt(self):
        ''' Compare 2 Rtt '''
        # self.rtt is better than Rtt(5)
        self.failUnless(self.rtt > Rtt(5))
        # Rtt(5) is worse than self.rtt
        self.failUnless(Rtt(5) < self.rtt)
        # self.rtt and Rtt(1) are the same
        self.failUnless(Rtt(1) == self.rtt)

    def testAddBwNullRem(self):
        ''' Sum a Bw with a NullRem '''
        new_bw = self.bw + self.null_rem
        self.failUnless(isinstance(new_bw, Bw))
        self.failUnlessEqual(new_bw.value,
                             min(self.bw.value, self.null_rem.value))

    def testAddBwDeadRem(self):
        ''' Sum a Rtt with a DeadRem '''
        self.failUnless(self.bw + self.dead_rem is self.dead_rem)

    def testCompareBw(self):
        ''' Compare 2 Bw '''

        # self.bw is worse than Bw(5)
        self.failUnless(self.bw < Bw(5, 1, 1))
        # Bw(5) is better than self.bw
        self.failUnless(Bw(5, 1, 1) > self.bw)
        # self.bw and Bw(1) are the same
        self.failUnless(Bw(1, 1, 1) == self.bw)

    def testAvgInit(self):
        ''' Check Avg initialization '''
        self.failUnlessEqual(self.avg.value, 59994)

    def testAvgInitError(self):
        ''' Check Avg initialization error '''
        self.assertRaises(RemError, Avg,
                          [Rtt(1), Rtt(5), 'Not a Rem instance'])

    def testCompareAvg(self):
        ''' Compare 2 Avg '''
        avg2 = Avg([Rtt(30), Rtt(25), Rtt(15)]) # avg2.value == 59976

        self.failUnless(avg2 < self.avg)
        self.failUnless(self.avg > avg2)
        self.failUnless(self.avg == Avg([Rtt(1), Rtt(5), Rtt(10)]))

    def testAddAvg(self):
        ''' Try to sum 2 Avg must fail '''

        self.assertRaises(AvgSumError, add, self.avg, self.avg)

class TestRouteGw(unittest.TestCase):

    def setUp(self):
        self.route_gw = RouteGw(object(), Rtt(1))

    def testCompareRouteGw(self):
        ''' Compare RouteGw '''
        self.failUnless(self.route_gw > RouteGw(object(), Rtt(10)))
        self.assertRaises(RouteGwError, lambda:self.route_gw == None)

    def testRemModify(self):
        ''' Modify a rem'''
        self.failUnless(self.route_gw.rem_modify(Rtt(1)) == Rtt(1))
        self.failUnless(self.route_gw.rem_modify(Rtt(10)) == Rtt(1))
        self.failUnless(self.route_gw.rem == Rtt(10))

class TestRouteNode(unittest.TestCase):

    def setUp(self):
        self.route_node = RouteNode()

    def testRouteAdd(self):
        ''' Add a route '''
        self.failUnlessEqual(self.route_node.routes, [])
        self.route_node.route_add(lvl=0, dst=1234, gw=1, rem=Rtt(20))
        self.failUnless(isinstance(self.route_node.routes[0], RouteGw))
        self.failUnlessEqual(self.route_node.routes[0].gw, 1)

    def testRouteGetbyGw(self):
        ''' Getting a route having as gateway "gw" '''
        self.failUnlessEqual(self.route_node.route_getby_gw(5), None)
        self.route_node.route_add(lvl=0, dst=123, gw=5, rem=Rtt(1))
        gw5 = self.route_node.route_getby_gw(5)
        self.failUnlessEqual(gw5.gw, 5)

    def testChangeRouteRem(self):
        ''' Change route rem '''
        self.route_node.route_add(lvl=0, dst=123, gw=5, rem=Rtt(50))

        res = self.route_node.route_rem(0, 0, 1, Rtt(10)) # Route don't exist
        self.failUnlessEqual(res, (0, None))

        res = self.route_node.route_rem(0, 0, 5, Rtt(10))
        self.failUnlessEqual(res, (1, Rtt(50)))


if __name__ == '__main__':
    unittest.main()
