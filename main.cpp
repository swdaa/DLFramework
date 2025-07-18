#include <gtest/gtest.h>
#include <glog/logging.h>

int main(int argc, char *argv[])
{

    testing::InitGoogleTest(&argc, argv);
    google::InitGoogleLogging("");
    FLAGS_log_dir = "../../part1";
    FLAGS_alsologtostderr = true;
    LOG(INFO) << "Start test ...\n";
    return RUN_ALL_TESTS();
}
