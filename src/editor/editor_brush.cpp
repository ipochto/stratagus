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
//      (c) Copyright 2023-2025 by Alyokhin
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

#include "editor.h"
#include "iolib.h"

/*----------------------------------------------------------------------------
--  Defines
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

CBrush::CBrush(std::string name, CBrush::Properties properties)
	: name(std::move(name)), properties(std::move(properties))
{
	rndEnabled = this->properties.randomizeAllowed;
	fixNeighborsEnabled = this->properties.fixNeighborsAllowed;

	// init generator's options
	if (!this->properties.decorationGenerator.options.empty()) {
		for (auto &[option, values] : this->properties.decorationGenerator.options) {
			if (values.empty()) {
				continue;
			}
			decorationOptions[option] = values[0];
		}
	} else {
		setSize(this->properties.minSize.width, this->properties.minSize.height);
	}
}

CBrush::CBrush(std::string name,
				CBrush::Properties properties,
				const std::vector<tile_index> &tilesSrc)
	: CBrush(std::move(name), std::move(properties))
{
	if (this->properties.decorationGenerator.options.empty()) {
		fillWith(tilesSrc);
	}
}

void CBrush::applyAt(const TilePos &pos, brushApplyFn applyFn, bool forbidRandomization /* = false*/) const
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
				const tile_index applyTile = forbidRandomization || !this->rndEnabled
											 ? tileIdx
											 : randomizeTile(tileIdx);
				applyFn(tileOffset, applyTile, isFixNeighborsEnabled(), isDecorative());
			}
		}
	}
}

tile_index CBrush::randomizeTile(tile_index tileIdx) const
{
	return Map.Tileset.getRandomTileOfTheSameKindAs(tileIdx);
}

graphic_index CBrush::getGraphicTile(uint8_t col, uint8_t row) const
{
	return getTile(col, row) ? Map.Tileset.getGraphicTileFor(getTile(col, row)) : 0;
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
		case EBrushTypes::SingleTile:
			fillWith(tile, !isInit);
			break;
		default:
			if (withinBounds(col, row)) {
				tiles[col + row * width] = tile;
			}
	}
}
void  CBrush::setTiles(uint8_t srcWidth, uint8_t srcHeight, const std::vector<tile_index> &srcTiles)
{
	setSize(srcWidth, srcHeight);
	fillWith(srcTiles);
}

void CBrush::fillWith(tile_index tile, bool init /* = false */)
{
	if (init && properties.shape == EBrushShapes::Round) {
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

/// Manual activated randomization of brush's tiles.
void CBrush::randomize()
{
	if (!properties.randomizeAllowed) {
		return;
	}
	for (auto &tile: tiles){
		tile = randomizeTile(tile);
	}
}

TilePos CBrush::getAlignOffset() const
{
	TilePos alignOffset{};
	if (isCentered()) {
		alignOffset.x = -getWidth() / 2;
		alignOffset.y = -getHeight() / 2;
	}
	return alignOffset;
}

void CBrush::setSize(uint8_t newWidth, uint8_t newHeight)
{
	width = newWidth;
	height = newHeight;
	tiles.resize(width * height, 0);
}

void CBrush::resizeW(uint8_t newWidth)
{
	resize(newWidth, this->height);
}

void CBrush::resizeH(uint8_t newHeight)
{
	resize(this->width, newHeight);
}

void CBrush::resize(uint8_t newWidth, uint8_t newHeight)
{
	if (!properties.resizable) {
		return;
	}
	if (newWidth != this->width && properties.resizeSteps.width == 0) {
		return;
	}
	if (newHeight != this->height && properties.resizeSteps.height == 0) {
		return;
	}

	const auto currentTile = properties.type == EBrushTypes::SingleTile ? getCurrentTile() : 0;

	tiles.clear();
	width = newWidth;
	height = newHeight;

	if(properties.shape == EBrushShapes::Round) {
		if (newWidth && newWidth % 2 == 0) {
			width = newWidth - 1;
		}
		height = width;
	}
	tiles.resize(width * height, 0);

	if (properties.type == EBrushTypes::SingleTile) {
		fillWith(currentTile, true);
	}
	/// FIXME: Init|fill
}

void CBrush::updateDecorationOption(const TDecorationOptionName &option, const TDecorationOptionValue &value)
{
	if (properties.decorationGenerator.options.count(option)
		&& ranges::contains(properties.decorationGenerator.options[option], value)) {

		decorationOptions[option] = value;
		loadDecoration();
	}
}

const CBrush::TDecorationOptionValue& CBrush::getDecorationOption(const TDecorationOptionName &option)
{
	static const TDecorationOptionValue emptyValue{"no value"};

	if (auto it = decorationOptions.find(option); it != decorationOptions.end()) {
		return it->second;
	}
	return emptyValue;
}

auto& CBrush::getDecoration(const TDecorationOptions &options) {
	static SDecoration emptyValue{};
	if (!decorationsPalette.count(options)) {
		const auto prevSize = decorationsPalette.size();
		generateDecoration();
		if (decorationsPalette.size() == prevSize) {
			ErrorPrint("Unable to generate decoration with current set of options\n");
			return emptyValue;
		}
	}
	return decorationsPalette[options];
}

void CBrush::pushDecorationTiles(uint8_t srcWidth, uint8_t srcHeight, const std::vector<tile_index> &srcTiles)
{
	if (srcWidth * srcHeight != srcTiles.size()) {
		return;
	}
	decorationsPalette[decorationOptions] = {srcWidth, srcHeight, srcTiles};
}

void CBrush::loadDecoration()
{
	const auto &[srcWidth, srcHeight, srcTiles] = getDecoration(decorationOptions);
	setTiles(srcWidth, srcHeight, srcTiles);
}

void CBrush::generateDecoration()
{
	const fs::path filename = LibraryFileName(properties.decorationGenerator.source);

	if (LuaLoadFile(filename) == -1) {
		ErrorPrint("%s's brush generator file '%s' not found\n",
					name.c_str(),
					properties.decorationGenerator.source.c_str());
	}
}

tile_index CBrush::getCurrentTile() const
{
	for (const auto &tile : tiles) {
		if (tile) {
			return tile;
		}
	}
	return 0;
}

// Bresenham algorithm
void CBrush::drawCircle(int16_t xCenter,
						int16_t yCenter,
						int16_t diameter,
						tile_index tile,
						std::vector<tile_index> &canvas)
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

void CBrushesSet::loadBrushes(std::string_view brushesSrc /* = {} */)
{
	brushes.clear();
	if (!brushesSrc.empty()) {
		this->brushesSrc = brushesSrc;
		const fs::path filename = LibraryFileName(this->brushesSrc);
		if (LuaLoadFile(filename) == -1) {
			ErrorPrint("Load failed: \"%s\"\n", filename.u8string().c_str());
			this->brushesSrc = {};
		}
	}
	if (brushes.empty()) {
		brushes.push_back(CBrush(std::string("Default"), CBrush::Properties()));
	}
	setCurrentBrush(brushes.begin()->getName());
}

bool CBrushesSet::setCurrentBrush(std::string_view name)
{
	Assert(!brushes.empty());

	const auto prev = currentBrush.getName();

	if (auto it =
	        ranges::find_if(brushes, [&](const auto &brush) { return name == brush.getName(); });
	    it != brushes.end()) {
			
		currentBrush = *it;
		const auto selectedTile = Editor.tileIcons.getSelectedTile();

		if (selectedTile && currentBrush.getType() == CBrush::EBrushTypes::SingleTile) {
			currentBrush.setTile(*selectedTile);
		} else if (currentBrush.getType() == CBrush::EBrushTypes::Decoration) {
			currentBrush.loadDecoration();
		}
	}
	if (prev == currentBrush.getName()) {
		return false;
	}
	return true;
};

std::vector<std::string> CBrushesSet::getBrushesNames() const
{
	std::vector<std::string> result;

	for (const auto &brush : brushes) {
		result.push_back(brush.getName());
	}
	return result;
}
//@}
