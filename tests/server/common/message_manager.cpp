#include "server/common/message_manager.h"

#include <fmt/core.h>
#include <gtest/gtest.h>

TEST(FormatLikePython, WithoutNamedArguments) {
    EXPECT_EQ("FOO", FormatLikePython("FOO"));
    EXPECT_EQ("I have a pen.", FormatLikePython("I have a pen."));
}
TEST(FormatLikePython, WithNamedArguments) {
    EXPECT_EQ("FOO BAR", FormatLikePython("FOO {another}", fmt::arg("another", "BAR")));
    EXPECT_EQ("I picked up a pen when walking.",
              FormatLikePython("I picked up {what} when {when}.", fmt::arg("when", "walking"),
                               fmt::arg("what", "a pen")));
}
TEST(FormatLikePython, PassInvalidNamedArguments) {
    EXPECT_THROW(FormatLikePython("FOO {another}"), fmt::format_error);
    EXPECT_THROW(FormatLikePython("FOO {another}", fmt::arg("the other", "BAR")),
                 fmt::format_error);
}
TEST(FormatLikePython, PassTooMuchNamedArguments) {
    EXPECT_EQ("FOO BAR", FormatLikePython("FOO {another}", fmt::arg("the other", "BAZ"),
                                          fmt::arg("another", "BAR")));
}
TEST(MessageManager, Initialization) {
    auto j = nlohmann::json();
    j["a"]["code"] = "CODE";
    j["a"]["message"] = "MESSAGE";
    MessageManager::InitializeFromJson(j);

    EXPECT_EQ("CODE", MessageManager::GetMessage("a").code);
    EXPECT_EQ("MESSAGE", MessageManager::GetMessage("a").message);
}
