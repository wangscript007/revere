
#include "test_r_utils_r_sha_256.h"
#include "r_utils/r_sha_256.h"

using namespace std;
using namespace r_utils;

REGISTER_TEST_FIXTURE(test_r_utils_r_sha_256);

void test_r_utils_r_sha_256::setup()
{
}

void test_r_utils_r_sha_256::teardown()
{
}

void test_r_utils_r_sha_256::test_hash_basic()
{
    r_sha_256 sha256;
    string msg = "Beneath this mask there is an idea, Mr. Creedy, and ideas are bulletproof.";
    sha256.update((uint8_t*)msg.c_str(), msg.length());
    sha256.finalize();
    auto output = sha256.get_as_string();
    RTF_ASSERT( output == "ddeb9f4cbc687d0a49d0ae60a683e0b62d3e4e27d91404690d37690e890f1d5c" );
}
