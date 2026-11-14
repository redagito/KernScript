#include "../../../Samples/Daybook/source/model.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Daybook rejects invalid mutations without consuming IDs or changing saved state", "[unit][samples][daybook]")
{
    daybook::Model model;
    daybook::Task task;
    task.title = "First task";
    const auto id = model.add(task);
    model.clearDirty();
    const auto original = model.tasks();
    const auto next = model.nextId();
    task.deadline = "2026-02-30";
    REQUIRE_THROWS_AS(model.add(task), std::invalid_argument);
    REQUIRE_THROWS_AS(model.edit(id, task), std::invalid_argument);
    REQUIRE(model.tasks() == original);
    REQUIRE(model.nextId() == next);
    REQUIRE_FALSE(model.dirty());
    task.deadline = "2028-02-29";
    REQUIRE(model.edit(id, task));
    REQUIRE(model.complete(id, true));
    REQUIRE(model.findTask(id)->completed);
    REQUIRE(model.erase(id));
    REQUIRE_FALSE(model.findTask(id));
}

TEST_CASE("Daybook storage limits reject edits atomically and release capacity after deletion",
          "[unit][samples][daybook]")
{
    daybook::Model model;
    daybook::Task small;
    small.title = "Small task";
    const auto first = model.add(small);
    daybook::Task large;
    large.title = std::string(512, '"');
    large.tags = std::string(512, '\\');
    large.kind = std::string(64, '"');
    bool full = false;
    for (std::size_t i = 0; i < daybook::Model::maxTasks; ++i)
    {
        try
        {
            model.add(large);
        }
        catch (const std::length_error &)
        {
            full = true;
            break;
        }
    }
    REQUIRE(full);
    for (int i = 0; i < 200; ++i)
    {
        try
        {
            model.add(small);
        }
        catch (const std::length_error &)
        {
            break;
        }
    }
    const auto count = model.tasks().size();
    const auto next = model.nextId();
    model.clearDirty();
    REQUIRE_THROWS_AS(model.edit(first, large), std::length_error);
    REQUIRE(model.findTask(first)->title == small.title);
    REQUIRE(model.tasks().size() == count);
    REQUIRE(model.nextId() == next);
    REQUIRE_FALSE(model.dirty());
    const auto last = model.tasks().back().id;
    REQUIRE(model.erase(last));
    REQUIRE_NOTHROW(model.add(small));
    REQUIRE(model.tasks().size() == count);
}

TEST_CASE("Daybook deletion retains the original target when resolving an editor changes selection",
          "[unit][samples][daybook]")
{
    daybook::Model model;
    daybook::Task task;
    task.title = "First task";
    const auto first = model.add(task);
    task.title = "Second task";
    const auto second = model.add(task);
    std::uint64_t selected = 0;
    bool resolved = false;
    REQUIRE_FALSE(daybook::eraseAfterResolving(model, selected,
                                               [&]
                                               {
                                                   resolved = true;
                                                   selected = first;
                                                   return true;
                                               }));
    REQUIRE_FALSE(resolved);
    REQUIRE(model.tasks().size() == 2);
    selected = first;
    REQUIRE(daybook::eraseAfterResolving(model, selected,
                                         [&]
                                         {
                                             selected = second;
                                             return true;
                                         }));
    REQUIRE_FALSE(model.findTask(first));
    REQUIRE(model.findTask(second));
    REQUIRE_FALSE(daybook::eraseAfterResolving(model, second, [] { return false; }));
    REQUIRE(model.findTask(second));
}
