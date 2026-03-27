#include "../Tests/PerformanceTest.cpp"

int main()
{
    std::cout << "Starting performance tests..." << std::endl;
    PerformanceTest::warmup();
    PerformanceTest::testSmallAllocation();
    PerformanceTest::testMultiThreaded();
    PerformanceTest::testMixedSizes();
    return 0;
}
