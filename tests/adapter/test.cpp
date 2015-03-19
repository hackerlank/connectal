/* Copyright (c) 2014 Quanta Research Cambridge, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "TestRequest.h"
#include "TestIndication.h"

class TestIndication : public TestIndicationWrapper
{
public:
    void done() {
        printf("Test: all done\n");
        exit(0);
    }
    TestIndication(unsigned int id) : TestIndicationWrapper(id) {}
};

int main(int argc, const char **argv)
{
    TestIndication *testIndication = new TestIndication(IfcNames_TestIndicationH2S);
    TestRequestProxy *testRequest = new TestRequestProxy(IfcNames_TestRequestS2H);

    printf("Test: start\n");
    testRequest->start();
    sleep(100);
    printf("Test: timed out\n");
    return 0;
}
