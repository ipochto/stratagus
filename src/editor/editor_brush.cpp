//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name editor_brush.h - Assistant for brushes in the editor. */
//
//      (c) Copyright 2023-2024 by Alyokhin
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "editor_brush.h"
#include "util.h"
#include "map.h"


/*----------------------------------------------------------------------------
--  Defines
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

void CBrush::applyBrushAt(const TilePos &pos, brushApplyFn applyFn) const
{
	TilePos brushOffset{};
	if (isCentered()) {
		brushOffset.x = int16_t(width) / 2;
		brushOffset.y = int16_t(height) / 2;
	}
	for (int16_t row = 0; row < height; ++row) {
		for (int16_t col = 0; col < width; ++col) {
			const tile_index tileIdx = getTile(col, row);
			if (tileIdx) {
				const TilePos tileOffset(col - brushOffset.x, row - brushOffset.y);
				applyFn(tileOffset, tileIdx);
			}
		}
	}
}

graphic_index CBrush::getGraphicTile(uint8_t col, uint8_t row) const
{
	return getTile(col, row) ? Map.Tileset->getGraphicTileFor(getTile(col, row)) : 0;
}

tile_index CBrush::getTile(uint8_t col, uint8_t row) const
{
	if (tiles.size() == 0 || !withinBounds(col, row)) {
		return 0;
	}
	return tiles[col + row * width];
}

void CBrush::setTile(tile_index tile, uint8_t col /* = 0 */, uint8_t row /* = 0 */)
{
	switch (properties.type)
	{
		case BrushTypes::SingleTile:
			fillWith(tile, isInit ? false : true);
			break;
		default:
			if (withinBounds(col, row)) {
				tiles[col + row * width] = tile;
			}
	}
}

void CBrush::fillWith(tile_index tile, bool init /* = false */)
{
	if (init && properties.shape == BrushShapes::Round) {
		ranges::fill(tiles, 0);
		if (width == height) {
			drawCircle(width / 2, height / 2, width, tile, this->tiles);
		}
	} else {
		for (auto &brushTile : tiles) {
			if (brushTile || init) {
				brushTile = tile;
			}
		}
	}
	if (init) {
		isInit = true;
	}
}

void CBrush::fillWith(const std::vector<tile_index> &tilesSrc)
{
	if (tilesSrc.size() != tiles.size()) {
		return;
	}
	ranges::copy(tilesSrc, tiles.begin());
}

TilePos CBrush::getAllignOffset() const
{
	TilePos allignOffset{};
	if (isCentered()) {
		allignOffset.x = -getWidth() / 2;
		allignOffset.y = -getHeight() / 2;
	}
	return allignOffset;
}

void CBrush::setSize(uint8_t newWidth, uint8_t newHeight)
{
	tiles.clear();
	width = newWidth;
	height = newHeight;

	if(properties.shape == BrushShapes::Round) {
		if (newWidth && newWidth % 2 == 0) {
			width = newWidth - 1;
		}
		height = width;
	}
	tiles.resize(width * height, 0);
	/// FIXME: Init|fill
}


// Bresenham algorithm 
void CBrush::drawCircle(int16_t xCenter, int16_t yCenter, int16_t diameter, tile_index tile, std::vector<tile_index> &canvas)
{
	// Because of the symmetry with respect to the cursor position, the diameter must be odd
	if (canvas.size() < diameter * diameter || diameter % 2 == 0) {
		return;
	}

	auto drawHLine = [diameter, tile, &canvas](int16_t x1, int16_t x2, int16_t y)
	{
		const size_t idx = y * diameter;
		std::fill(&canvas[idx + x1], &canvas[idx + x2 + 1], tile);
	};

	int x = diameter / 2;
	int y = 0;
	int delta = 1 - x;
 
	while (x >= y)
	{
		drawHLine(xCenter - x, xCenter + x, yCenter + y);
		drawHLine(xCenter - x, xCenter + x, yCenter - y);
		drawHLine(xCenter - y, xCenter + y, yCenter + x);
		drawHLine(xCenter - y, xCenter + y, yCenter - x);

		y++;
		if (delta < 0)
		{
			delta += 2 * y + 1;
		}
		else
		{
			x--;
			delta += 2 * (y - x + 1);
		}
	}
}

//@}
