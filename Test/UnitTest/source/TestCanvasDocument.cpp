#include "../../../Samples/Canvas/source/Document.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Canvas strokes undo and redo across the saved revision", "[unit][samples][canvas]")
{
    canvas::Document doc;
    REQUIRE_FALSE(doc.dirty());
    doc.beginEdit();
    doc.image().pixels[0] = 0xff0000;
    doc.image().pixels[1] = 0x00ff00;
    REQUIRE(doc.commitEdit());
    doc.markSaved();
    doc.beginEdit();
    doc.image().pixels[0] = 0x0000ff;
    REQUIRE(doc.commitEdit());
    REQUIRE(doc.dirty());
    REQUIRE(doc.undo());
    REQUIRE(doc.image().pixels[0] == 0xff0000);
    REQUIRE_FALSE(doc.dirty());
    REQUIRE(doc.undo());
    REQUIRE(doc.image().pixels[1] == 0);
    REQUIRE(doc.dirty());
    REQUIRE(doc.redo());
    REQUIRE_FALSE(doc.dirty());
    REQUIRE(doc.redo());
    REQUIRE(doc.image().pixels[0] == 0x0000ff);
}

TEST_CASE("Canvas previews restart and cancel without damaging the document or redo", "[unit][samples][canvas]")
{
    canvas::Document doc;
    doc.beginEdit();
    doc.image().pixels[0] = 42;
    doc.commitEdit();
    doc.undo();
    doc.beginEdit();
    doc.image().pixels[0] = 99;
    REQUIRE_FALSE(doc.canUndo());
    REQUIRE_FALSE(doc.canRedo());
    REQUIRE_THROWS_AS(doc.beginEdit(), std::logic_error);
    REQUIRE_THROWS_AS(doc.markSaved(), std::logic_error);
    doc.restartPreview();
    REQUIRE(doc.image().pixels[0] == 0);
    doc.image().pixels[1] = 13;
    doc.cancelEdit();
    REQUIRE(doc.image().pixels[1] == 0);
    REQUIRE_FALSE(doc.dirty());
    REQUIRE(doc.redo());
    REQUIRE(doc.image().pixels[0] == 42);
}

TEST_CASE("Canvas no-op edits preserve redo and changed edits branch history", "[unit][samples][canvas]")
{
    canvas::Document doc;
    doc.beginEdit();
    doc.image().pixels[0] = 7;
    doc.commitEdit();
    doc.undo();
    doc.beginEdit();
    REQUIRE_FALSE(doc.commitEdit());
    REQUIRE(doc.canRedo());
    doc.beginEdit();
    doc.image().pixels[1] = 9;
    REQUIRE(doc.commitEdit());
    REQUIRE_FALSE(doc.canRedo());
}

TEST_CASE("Canvas image replacement validates before changing user work", "[unit][samples][canvas]")
{
    canvas::Document doc;
    doc.beginEdit();
    doc.image().pixels[0] = 7;
    doc.commitEdit();
    REQUIRE_THROWS_AS(doc.replace({3, 3, {1}}, true), std::invalid_argument);
    REQUIRE(doc.image().pixels[0] == 7);
    REQUIRE(doc.canUndo());
    REQUIRE_THROWS_AS(canvas::Image::blank(0, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(canvas::Image::blank(2049, 1), std::invalid_argument);
    doc.replace(canvas::Image::blank(640, 400), true);
    REQUIRE(doc.image().pixels.size() == 640 * 400);
    REQUIRE_FALSE(doc.dirty());
    REQUIRE_FALSE(doc.canUndo());
    doc.replace(canvas::Image::blank(960, 600), false);
    REQUIRE(doc.dirty());
}

TEST_CASE("Canvas history obeys a pixel memory budget", "[unit][samples][canvas]")
{
    canvas::Document doc(320 * 200 * sizeof(int));
    for (int i = 1; i <= 4; ++i)
    {
        doc.beginEdit();
        doc.image().pixels[0] = i;
        doc.commitEdit();
    }
    REQUIRE(doc.undo());
    REQUIRE(doc.image().pixels[0] == 3);
    REQUIRE_FALSE(doc.canUndo());
    REQUIRE(doc.redo());
    REQUIRE(doc.image().pixels[0] == 4);
}
