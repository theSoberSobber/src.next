// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/nine_piece_image_grid.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/style/nine_piece_image.h"
#include "third_party/blink/renderer/core/style/style_generated_image.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

class NinePieceImageGridTest : public RenderingTest {
 public:
  NinePieceImageGridTest() = default;

  StyleImage* GeneratedImage() {
    auto* gradient = MakeGarbageCollected<cssvalue::

                                              CSSLinearGradientValue>(
        nullptr, nullptr, nullptr, nullptr, nullptr, cssvalue::kRepeating);
    return MakeGarbageCollected<StyleGeneratedImage>(*gradient);
  }
};

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_NoDrawables) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());

  FloatSize image_size(100, 100);
  IntRect border_image_area(0, 0, 100, 100);
  IntRectOutsets border_widths(0, 0, 0, 0);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_FALSE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_AllDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));
  nine_piece.SetFill(true);

  FloatSize image_size(100, 100);
  IntRect border_image_area(0, 0, 100, 100);
  IntRectOutsets border_widths(10, 10, 10, 10);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_TRUE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_NoFillMiddleNotDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));
  nine_piece.SetFill(false);  // default

  FloatSize image_size(100, 100);
  IntRect border_image_area(0, 0, 100, 100);
  IntRectOutsets border_widths(10, 10, 10, 10);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    if (piece != kMiddlePiece)
      EXPECT_TRUE(draw_info.is_drawable);
    else
      EXPECT_FALSE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_EmptySidesNotDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(Length::Percent(49), Length::Percent(49),
                                      Length::Percent(49),
                                      Length::Percent(49)));

  FloatSize image_size(6, 6);
  IntRect border_image_area(0, 0, 6, 6);
  IntRectOutsets border_widths(3, 3, 3, 3);

  NinePieceImageGrid grid(nine_piece, image_size, FloatSize(1, 1), 1,
                          border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    auto draw_info = grid.GetNinePieceDrawInfo(piece);
    if (piece == kLeftPiece || piece == kRightPiece || piece == kTopPiece ||
        piece == kBottomPiece || piece == kMiddlePiece)
      EXPECT_FALSE(draw_info.is_drawable);
    else
      EXPECT_TRUE(draw_info.is_drawable);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_TopLeftDrawable) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));

  FloatSize image_size(100, 100);
  IntRect border_image_area(0, 0, 100, 100);

  const struct {
    IntRectOutsets border_widths;
    bool expected_is_drawable;
  } test_cases[] = {
      {IntRectOutsets(0, 0, 0, 0), false},
      {IntRectOutsets(10, 0, 0, 0), false},
      {IntRectOutsets(0, 0, 0, 10), false},
      {IntRectOutsets(10, 0, 0, 10), true},
  };

  for (const auto& test_case : test_cases) {
    NinePieceImageGrid grid =
        NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                           border_image_area, test_case.border_widths);
    for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
      NinePieceImageGrid::NinePieceDrawInfo draw_info =
          grid.GetNinePieceDrawInfo(piece);
      if (piece == kTopLeftPiece)
        EXPECT_EQ(draw_info.is_drawable, test_case.expected_is_drawable);
    }
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_ScaleDownBorder) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));

  FloatSize image_size(100, 100);
  IntRect border_image_area(0, 0, 100, 100);
  IntRectOutsets border_widths(10, 10, 10, 10);

  // Set border slices wide enough so that the widths are scaled
  // down and corner pieces cover the entire border image area.
  nine_piece.SetBorderSlices(BorderImageLengthBox(6));

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                         border_image_area, border_widths);
  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    if (draw_info.is_corner_piece)
      EXPECT_EQ(draw_info.destination.Size(), FloatSize(50, 50));
    else
      EXPECT_TRUE(draw_info.destination.Size().IsEmpty());
  }

  // Like above, but also make sure to get a scale-down factor that requires
  // rounding to pick the larger value on one of the edges. (A 1:3, 2:3 split.)
  BorderImageLength top_left(10);
  BorderImageLength bottom_right(20);
  nine_piece.SetBorderSlices(
      BorderImageLengthBox(top_left, bottom_right, bottom_right, top_left));
  grid = NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                            border_image_area, border_widths);
  NinePieceImageGrid::NinePieceDrawInfo draw_info =
      grid.GetNinePieceDrawInfo(kTopLeftPiece);
  EXPECT_EQ(draw_info.destination.Size(), FloatSize(33, 33));
  draw_info = grid.GetNinePieceDrawInfo(kTopRightPiece);
  EXPECT_EQ(draw_info.destination.Size(), FloatSize(67, 33));
  draw_info = grid.GetNinePieceDrawInfo(kBottomLeftPiece);
  EXPECT_EQ(draw_info.destination.Size(), FloatSize(33, 67));
  draw_info = grid.GetNinePieceDrawInfo(kBottomRightPiece);
  EXPECT_EQ(draw_info.destination.Size(), FloatSize(67, 67));

  // Set border slices that overlap in one dimension but not in the other, and
  // where the resulting width in the non-overlapping dimension will round to a
  // larger width.
  BorderImageLength top_bottom(10);
  BorderImageLength left_right(Length::Fixed(11));
  nine_piece.SetBorderSlices(
      BorderImageLengthBox(top_bottom, left_right, top_bottom, left_right));
  grid = NinePieceImageGrid(nine_piece, image_size, FloatSize(1, 1), 1,
                            border_image_area, border_widths);
  NinePieceImageGrid::NinePieceDrawInfo tl_info =
      grid.GetNinePieceDrawInfo(kTopLeftPiece);
  EXPECT_EQ(tl_info.destination.Size(), FloatSize(6, 50));
  // The top-right, bottom-left and bottom-right pieces are the same size as
  // the top-left piece.
  draw_info = grid.GetNinePieceDrawInfo(kTopRightPiece);
  EXPECT_EQ(tl_info.destination.Size(), draw_info.destination.Size());
  draw_info = grid.GetNinePieceDrawInfo(kBottomLeftPiece);
  EXPECT_EQ(tl_info.destination.Size(), draw_info.destination.Size());
  draw_info = grid.GetNinePieceDrawInfo(kBottomRightPiece);
  EXPECT_EQ(tl_info.destination.Size(), draw_info.destination.Size());
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting) {
  const struct {
    FloatSize image_size;
    IntRect border_image_area;
    IntRectOutsets border_widths;
    bool fill;
    LengthBox image_slices;
    ENinePieceImageRule horizontal_rule;
    ENinePieceImageRule vertical_rule;
    struct {
      bool is_drawable;
      bool is_corner_piece;
      FloatRect destination;
      FloatRect source;
      float tile_scale_horizontal;
      float tile_scale_vertical;
      ENinePieceImageRule horizontal_rule;
      ENinePieceImageRule vertical_rule;
    } pieces[9];
  } test_cases[] = {
      {// Empty border and slices but with fill
       FloatSize(100, 100),
       IntRect(0, 0, 100, 100),
       IntRectOutsets(0, 0, 0, 0),
       true,
       LengthBox(Length::Fixed(0), Length::Fixed(0), Length::Fixed(0),
                 Length::Fixed(0)),
       kStretchImageRule,
       kStretchImageRule,
       {
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(0, 0, 100, 100), FloatRect(0, 0, 100, 100),
            1, 1, kStretchImageRule, kStretchImageRule},
       }},
      {// Single border and fill
       FloatSize(100, 100),
       IntRect(0, 0, 100, 100),
       IntRectOutsets(0, 0, 10, 0),
       true,
       LengthBox(Length::Percent(20), Length::Percent(20), Length::Percent(20),
                 Length::Percent(20)),
       kStretchImageRule,
       kStretchImageRule,
       {
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(0, 90, 100, 10), FloatRect(20, 80, 60, 20),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(0, 0, 100, 90), FloatRect(20, 20, 60, 60),
            1.666667, 1.5, kStretchImageRule, kStretchImageRule},
       }},
      {// All borders, no fill
       FloatSize(100, 100),
       IntRect(0, 0, 100, 100),
       IntRectOutsets(10, 10, 10, 10),
       false,
       LengthBox(Length::Percent(20), Length::Percent(20), Length::Percent(20),
                 Length::Percent(20)),
       kStretchImageRule,
       kStretchImageRule,
       {
           {true, true, FloatRect(0, 0, 10, 10), FloatRect(0, 0, 20, 20), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {true, true, FloatRect(0, 90, 10, 10), FloatRect(0, 80, 20, 20), 1,
            1, kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(0, 10, 10, 80), FloatRect(0, 20, 20, 60),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, true, FloatRect(90, 0, 10, 10), FloatRect(80, 0, 20, 20), 1,
            1, kStretchImageRule, kStretchImageRule},
           {true, true, FloatRect(90, 90, 10, 10), FloatRect(80, 80, 20, 20), 1,
            1, kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(90, 10, 10, 80), FloatRect(80, 20, 20, 60),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(10, 0, 80, 10), FloatRect(20, 0, 60, 20),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(10, 90, 80, 10), FloatRect(20, 80, 60, 20),
            0.5, 0.5, kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kStretchImageRule},
       }},
      {// Single border, no fill
       FloatSize(100, 100),
       IntRect(0, 0, 100, 100),
       IntRectOutsets(0, 0, 0, 10),
       false,
       LengthBox(Length::Percent(20), Length::Percent(20), Length::Percent(20),
                 Length::Percent(20)),
       kStretchImageRule,
       kRoundImageRule,
       {
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {true, false, FloatRect(0, 0, 10, 100), FloatRect(0, 20, 20, 60),
            0.5, 0.5, kStretchImageRule, kRoundImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kRoundImageRule},
       }},
      {// All borders but no slices, with fill (stretch horizontally, space
       // vertically)
       FloatSize(100, 100),
       IntRect(0, 0, 100, 100),
       IntRectOutsets(10, 10, 10, 10),
       true,
       LengthBox(Length::Fixed(0), Length::Fixed(0), Length::Fixed(0),
                 Length::Fixed(0)),
       kStretchImageRule,
       kSpaceImageRule,
       {
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, true, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 1, 1,
            kStretchImageRule, kStretchImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {false, false, FloatRect(0, 0, 0, 0), FloatRect(0, 0, 0, 0), 0, 0,
            kStretchImageRule, kSpaceImageRule},
           {true, false, FloatRect(10, 10, 80, 80), FloatRect(0, 0, 100, 100),
            0.800000, 1, kStretchImageRule, kSpaceImageRule},
       }},
  };

  for (auto& test_case : test_cases) {
    NinePieceImage nine_piece;
    nine_piece.SetImage(GeneratedImage());
    nine_piece.SetFill(test_case.fill);
    nine_piece.SetImageSlices(test_case.image_slices);
    nine_piece.SetHorizontalRule(
        (ENinePieceImageRule)test_case.horizontal_rule);
    nine_piece.SetVerticalRule((ENinePieceImageRule)test_case.vertical_rule);

    NinePieceImageGrid grid = NinePieceImageGrid(
        nine_piece, test_case.image_size, FloatSize(1, 1), 1,
        test_case.border_image_area, test_case.border_widths);
    for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
      NinePieceImageGrid::NinePieceDrawInfo draw_info =
          grid.GetNinePieceDrawInfo(piece);
      EXPECT_EQ(test_case.pieces[piece].is_drawable, draw_info.is_drawable);
      if (!test_case.pieces[piece].is_drawable)
        continue;

      EXPECT_EQ(test_case.pieces[piece].destination.X(),
                draw_info.destination.X());
      EXPECT_EQ(test_case.pieces[piece].destination.Y(),
                draw_info.destination.Y());
      EXPECT_EQ(test_case.pieces[piece].destination.Width(),
                draw_info.destination.Width());
      EXPECT_EQ(test_case.pieces[piece].destination.Height(),
                draw_info.destination.Height());
      EXPECT_EQ(test_case.pieces[piece].source.X(), draw_info.source.X());
      EXPECT_EQ(test_case.pieces[piece].source.Y(), draw_info.source.Y());
      EXPECT_EQ(test_case.pieces[piece].source.Width(),
                draw_info.source.Width());
      EXPECT_EQ(test_case.pieces[piece].source.Height(),
                draw_info.source.Height());

      if (test_case.pieces[piece].is_corner_piece)
        continue;

      EXPECT_FLOAT_EQ(test_case.pieces[piece].tile_scale_horizontal,
                      draw_info.tile_scale.Width());
      EXPECT_FLOAT_EQ(test_case.pieces[piece].tile_scale_vertical,
                      draw_info.tile_scale.Height());
      EXPECT_EQ(test_case.pieces[piece].horizontal_rule,
                draw_info.tile_rule.horizontal);
      EXPECT_EQ(test_case.pieces[piece].vertical_rule,
                draw_info.tile_rule.vertical);
    }
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_Zoomed) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  // Image slices are specified in CSS pixels.
  nine_piece.SetImageSlices(LengthBox(10, 10, 10, 10));
  nine_piece.SetFill(true);

  FloatSize image_size(50, 50);
  IntRect border_image_area(0, 0, 200, 200);
  IntRectOutsets border_widths(20, 20, 20, 20);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, FloatSize(2, 2), 2,
                         border_image_area, border_widths);
  struct {
    bool is_drawable;
    bool is_corner_piece;
    FloatRect destination;
    FloatRect source;
    float tile_scale_horizontal;
    float tile_scale_vertical;
    ENinePieceImageRule horizontal_rule;
    ENinePieceImageRule vertical_rule;
  } expected_pieces[kMaxPiece] = {
      {true, true, FloatRect(0, 0, 20, 20), FloatRect(0, 0, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, true, FloatRect(0, 180, 20, 20), FloatRect(0, 30, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(0, 20, 20, 160), FloatRect(0, 20, 20, 10), 1, 1,
       kStretchImageRule, kStretchImageRule},
      {true, true, FloatRect(180, 0, 20, 20), FloatRect(30, 0, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, true, FloatRect(180, 180, 20, 20), FloatRect(30, 30, 20, 20), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(180, 20, 20, 160), FloatRect(30, 20, 20, 10), 1,
       1, kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(20, 0, 160, 20), FloatRect(20, 0, 10, 20), 1, 1,
       kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(20, 180, 160, 20), FloatRect(20, 30, 10, 20), 1,
       1, kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(20, 20, 160, 160), FloatRect(20, 20, 10, 10), 16,
       16, kStretchImageRule, kStretchImageRule},
  };

  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_TRUE(draw_info.is_drawable);

    const auto& expected = expected_pieces[piece];
    EXPECT_EQ(draw_info.destination, expected.destination);
    EXPECT_EQ(draw_info.source, expected.source);

    if (expected.is_corner_piece)
      continue;

    EXPECT_FLOAT_EQ(draw_info.tile_scale.Width(),
                    expected.tile_scale_horizontal);
    EXPECT_FLOAT_EQ(draw_info.tile_scale.Height(),
                    expected.tile_scale_vertical);
    EXPECT_EQ(draw_info.tile_rule.vertical, expected.vertical_rule);
    EXPECT_EQ(draw_info.tile_rule.horizontal, expected.horizontal_rule);
  }
}

TEST_F(NinePieceImageGridTest, NinePieceImagePainting_ZoomedNarrowSlices) {
  NinePieceImage nine_piece;
  nine_piece.SetImage(GeneratedImage());
  // Image slices are specified in CSS pixels.
  nine_piece.SetImageSlices(LengthBox(1, 1, 1, 1));
  nine_piece.SetFill(true);

  constexpr float zoom = 2.2f;
  FloatSize image_size(3 * zoom, 3 * zoom);
  IntRect border_image_area(0, 0, 220, 220);
  IntRectOutsets border_widths(33, 33, 33, 33);

  NinePieceImageGrid grid =
      NinePieceImageGrid(nine_piece, image_size, FloatSize(zoom, zoom), zoom,
                         border_image_area, border_widths);
  struct {
    bool is_drawable;
    bool is_corner_piece;
    FloatRect destination;
    FloatRect source;
    float tile_scale_horizontal;
    float tile_scale_vertical;
    ENinePieceImageRule horizontal_rule;
    ENinePieceImageRule vertical_rule;
  } expected_pieces[kMaxPiece] = {
      {true, true, FloatRect(0, 0, 33, 33), FloatRect(0, 0, 2.2f, 2.2f), 0, 0,
       kStretchImageRule, kStretchImageRule},
      {true, true, FloatRect(0, 187, 33, 33), FloatRect(0, 4.4f, 2.2f, 2.2f), 0,
       0, kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(0, 33, 33, 154), FloatRect(0, 2.2f, 2.2f, 2.2f),
       15, 15, kStretchImageRule, kStretchImageRule},
      {true, true, FloatRect(187, 0, 33, 33), FloatRect(4.4f, 0, 2.2f, 2.2f), 0,
       0, kStretchImageRule, kStretchImageRule},
      {true, true, FloatRect(187, 187, 33, 33),
       FloatRect(4.4f, 4.4f, 2.2f, 2.2f), 0, 0, kStretchImageRule,
       kStretchImageRule},
      {true, false, FloatRect(187, 33, 33, 154),
       FloatRect(4.4f, 2.2f, 2.2f, 2.2f), 15, 15, kStretchImageRule,
       kStretchImageRule},
      {true, false, FloatRect(33, 0, 154, 33), FloatRect(2.2f, 0, 2.2f, 2.2f),
       15, 15, kStretchImageRule, kStretchImageRule},
      {true, false, FloatRect(33, 187, 154, 33),
       FloatRect(2.2f, 4.4f, 2.2f, 2.2f), 15, 15, kStretchImageRule,
       kStretchImageRule},
      {true, false, FloatRect(33, 33, 154, 154),
       FloatRect(2.2f, 2.2f, 2.2f, 2.2f), 70, 70, kStretchImageRule,
       kStretchImageRule},
  };

  for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
    NinePieceImageGrid::NinePieceDrawInfo draw_info =
        grid.GetNinePieceDrawInfo(piece);
    EXPECT_TRUE(draw_info.is_drawable);

    const auto& expected = expected_pieces[piece];
    EXPECT_FLOAT_EQ(draw_info.destination.X(), expected.destination.X());
    EXPECT_FLOAT_EQ(draw_info.destination.Y(), expected.destination.Y());
    EXPECT_FLOAT_EQ(draw_info.destination.Width(),
                    expected.destination.Width());
    EXPECT_FLOAT_EQ(draw_info.destination.Height(),
                    expected.destination.Height());
    EXPECT_FLOAT_EQ(draw_info.source.X(), expected.source.X());
    EXPECT_FLOAT_EQ(draw_info.source.Y(), expected.source.Y());
    EXPECT_FLOAT_EQ(draw_info.source.Width(), expected.source.Width());
    EXPECT_FLOAT_EQ(draw_info.source.Height(), expected.source.Height());

    if (expected.is_corner_piece)
      continue;

    EXPECT_FLOAT_EQ(draw_info.tile_scale.Width(),
                    expected.tile_scale_horizontal);
    EXPECT_FLOAT_EQ(draw_info.tile_scale.Height(),
                    expected.tile_scale_vertical);
    EXPECT_EQ(draw_info.tile_rule.vertical, expected.vertical_rule);
    EXPECT_EQ(draw_info.tile_rule.horizontal, expected.horizontal_rule);
  }
}

}  // namespace
}  // namespace blink
