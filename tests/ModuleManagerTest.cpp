#include <gtest/gtest.h>

#include "ModuleManager.hpp"
#include "graphic.hpp"

struct ModuleManagerTest : ::testing::Test {
    ModuleManager<IGraphicModule, IWindowModule> modules;
};

TEST_F(ModuleManagerTest, LoadDiscoversModules) {
    ASSERT_TRUE(modules.Load(RAY_PATH, "ray"));
    EXPECT_NE(modules.Get<IGraphicModule>("ray"), nullptr);
}

TEST_F(ModuleManagerTest, LoadingSameKeyTwiceIsRefused) {
    EXPECT_TRUE(modules.Load(RAY_PATH, "ray"));
    EXPECT_FALSE(modules.Load(RAY_PATH, "ray"));
}

/* The same dll under two keys is two rows : nothing says a key is a file. */
TEST_F(ModuleManagerTest, SameLibraryUnderTwoKeysIsTwoRows) {
    ASSERT_TRUE(modules.Load(RAY_PATH, "ray"));
    ASSERT_TRUE(modules.Load(RAY_PATH, "ray-again"));
    EXPECT_EQ(modules.GetAll<IGraphicModule>().size(), 2u);
}

/* The key is the ONLY name of a row. What the module calls itself is never
 * read by the manager, so it cannot be used to look one up. */
TEST_F(ModuleManagerTest, LookupUsesTheKeyNotWhatTheModuleCallsItself) {
    ASSERT_TRUE(modules.Load(RAY_PATH, "first-vendor"));

    EXPECT_NE(modules.Get<IGraphicModule>("first-vendor"), nullptr);
    EXPECT_EQ(modules.Get<IGraphicModule>("ray"), nullptr);
}

TEST_F(ModuleManagerTest, GetByUnknownKeyIsNull) {
    modules.Load(RAY_PATH, "ray");
    EXPECT_EQ(modules.Get<IGraphicModule>("vulkan"), nullptr);
}

TEST_F(ModuleManagerTest, PartialCoverageIsNotAnError) {
    modules.Load(SFML_PATH, "sfml"); /* does not provide window */
    EXPECT_NE(modules.Get<IGraphicModule>("sfml"), nullptr);
    EXPECT_EQ(modules.Get<IWindowModule>("sfml"), nullptr);
}

TEST_F(ModuleManagerTest, GetAllListsEveryLibraryProvidingTheContract) {
    modules.Load(RAY_PATH, "ray");
    modules.Load(SFML_PATH, "sfml");

    EXPECT_EQ(modules.GetAll<IGraphicModule>().size(), 2u);
    EXPECT_EQ(modules.GetAll<IWindowModule>().size(), 1u); /* sfml has none */
}

TEST_F(ModuleManagerTest, UnloadRemovesModulesThenClosesTheLibraryInOneCall) {
    modules.Load(SFML_PATH, "sfml");

    modules.Unload("sfml");

    EXPECT_EQ(modules.Get<IGraphicModule>("sfml"), nullptr);
    EXPECT_TRUE(modules.GetAll<IGraphicModule>().empty());
    EXPECT_FALSE(modules.Find("sfml").has_value());
}

TEST_F(ModuleManagerTest, UnloadingOneLeavesTheOtherIntact) {
    modules.Load(RAY_PATH, "ray");
    modules.Load(SFML_PATH, "sfml");

    modules.Unload("ray");

    EXPECT_EQ(modules.Get<IGraphicModule>("ray"), nullptr);
    EXPECT_NE(modules.Get<IGraphicModule>("sfml"), nullptr);
}

TEST_F(ModuleManagerTest, UnloadingAnUnknownKeyIsANoOp) {
    modules.Load(RAY_PATH, "ray");
    modules.Unload("vulkan");
    EXPECT_NE(modules.Get<IGraphicModule>("ray"), nullptr);
}

TEST_F(ModuleManagerTest, GetByEntityReturnsTheModuleOfThatRow) {
    modules.Load(RAY_PATH, "ray");

    auto e = modules.Find("ray");

    ASSERT_TRUE(e.has_value());
    EXPECT_NE(modules.Get<IGraphicModule>(*e), nullptr);
    EXPECT_EQ(modules.Get<IGraphicModule>(*e), modules.Get<IGraphicModule>("ray"));
}

TEST_F(ModuleManagerTest, GetByEntityReturnsTheSharedLibraryOfThatRow) {
    modules.Load(RAY_PATH, "ray");

    auto e = modules.Find("ray");

    ASSERT_TRUE(e.has_value());
    ASSERT_NE(modules.Get(*e), nullptr);
    EXPECT_EQ(modules.Get(*e)->path(), RAY_PATH);
}

/* A freed row is reused by the next Load : the old Entity must not resolve
 * to the new library. */
TEST_F(ModuleManagerTest, ReloadingAfterUnloadGivesAFreshRow) {
    modules.Load(RAY_PATH, "ray");
    modules.Unload("ray");
    modules.Load(SFML_PATH, "sfml");

    EXPECT_FALSE(modules.Find("ray").has_value());
    EXPECT_NE(modules.Get<IGraphicModule>("sfml"), nullptr);
    EXPECT_EQ(modules.Get<IWindowModule>("sfml"), nullptr);
}

TEST_F(ModuleManagerTest, GetAllByKeyListsEveryContractThatLibraryProvides) {
    modules.Load(RAY_PATH, "ray");   /* graphic + window */
    modules.Load(SFML_PATH, "sfml"); /* graphic only    */

    EXPECT_EQ(modules.GetAllByKey("ray").size(), 2u);
    EXPECT_EQ(modules.GetAllByKey("sfml").size(), 1u);
    EXPECT_TRUE(modules.GetAllByKey("vulkan").empty());
}

TEST_F(ModuleManagerTest, GetAllWithNoArgumentListsEveryModuleOfEveryLibrary) {
    modules.Load(RAY_PATH, "ray");   /* graphic + window */
    modules.Load(SFML_PATH, "sfml"); /* graphic only    */

    EXPECT_EQ(modules.GetAll().size(), 3u);

    modules.Unload("ray");
    EXPECT_EQ(modules.GetAll().size(), 1u);
}
