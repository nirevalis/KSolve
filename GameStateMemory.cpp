// GameStateMemory.cpp implements the GameStateMemory class.

#include <algorithm>        // max
#include "GameStateMemory.hpp"

namespace KSolveNames {

// 32 bytes --> 21 bits
static inline uint32_t DeflateTableau(const Pile& cards) noexcept
{
    uint32_t result {0};
    const unsigned upCount = cards.UpCount();
    if (upCount) {
        // The rules for moving to the tableau piles guarantee
        // all the face-up cards in such a pile can be identified
        // by identifying the bottom card (the first face-up card)
        // and whether each other face-up card is from 
        // a major suit (hearts or spades) or not.
        //
        // The face-up cards in a tableau pile cannot number
        // more than 12, since AvailableMoves() will never move an
        // ace there.
        unsigned isMajor = 
            std::accumulate(cards.end()-upCount+1, cards.end(), 0,
                [](unsigned acc, Card card)
                    {return acc<<1 | card.IsMajor();});
        const Card top = cards.Top();
        result =  ((top.Suit()
                    <<4  | top.Rank())
                    <<11 | isMajor)
                    <<4  | upCount;
    }
    return result;
}
GameState::GameState(const Game& game, unsigned moveCount) noexcept
    : _moveCount(moveCount)
{
    std::array<uint32_t,TableauSize> tableauState;
    const auto& tableau = game.Tableau();
    for (unsigned i = 0; i<TableauSize; ++i) {
        tableauState[i] = DeflateTableau(tableau[i]);
    }
    // Sort the tableau states because tableaus that are identical
    // except for order are considered equal
    ranges::sort(tableauState);

    _part0 =          (PartType(tableauState[0])
                <<21 | PartType(tableauState[1]))
                <<21 | PartType(tableauState[2]);
    _part1 =          (PartType(tableauState[3])
                <<21 | PartType(tableauState[4]))
                <<21 | PartType(tableauState[5]);
    auto& fnd{game.Foundation()};
    _part2 =       ((((PartType(tableauState[6])
                <<5  | game.StockPile().size())
                <<4  | fnd[0].size())
                <<4  | fnd[1].size()) 
                <<4  | fnd[2].size()) 
                <<4  | fnd[3].size();
}

GameStateMemory::GameStateMemory() noexcept
    : _states()
{
    _states.reserve(MinCapacity);
}

bool GameStateMemory::IsShortPathToState(const Game& game, unsigned moveCount) noexcept
{
    const GameState newState{game,moveCount};
    bool valueChanged{false};
    bool isNewKey = _states.lazy_emplace_l(
        newState,						// (key, value)
        [&](auto& oldState) {	// run behind lock when key found
            if (moveCount < oldState._moveCount) {
                oldState._moveCount = moveCount;
                valueChanged = true;
            }
        },
        [&](const MapType::constructor& ctor) { // ... if key not found
            ctor(newState);
        }
    );
    return isNewKey | valueChanged;
}
}   // namespace KSolveNames