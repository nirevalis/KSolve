// A GameStateMemory instance stores {GameState, nMoves} pairs
// so that a solver can determine whether the current game state
// has been encountered before, and if it has, was the path
// to it as short as the current path.  The object keeps the
// length of the shortest path to each state encountered so far.
//
// Instances are thread-safe.

#include "Game.hpp"                     // for Game
#include "parallel_hashmap/phmap.h"     // for parallel_flat_hash_set
#include <mutex>
namespace KSolveNames {
// A compact representation of the current game state.
//
// For game play purposes, two tableaus that are identical except
// that one or more piles are in different spots are considered
// equal.  Two game states are defined as equal here if their
// foundation piles and stock and waste piles are the same and
// their tableaus are equal except for order of piles.
//
// The basic requirements for GameState are:
// 1.  Any difference in between game states as defined above
//     must be reflected in the corresponding GameState objects.
//     A GameState is a perfect hash of the game state given
//     that equivalence relation.
// 2.  It should be quite compact, as we will usually be storing
//     millions or tens of millions of instances.
//
// Conceptually, this is a hash map where the move count is the 
// value and game state is the key.  In order to save space,
// it is implemented as a hash set so the value can be packed in
// with the key.  The hash and compare functions operate only on
// the key.
struct GameState {
    using PartType = std::uint64_t;
    PartType _part0;            // key[0]
    PartType _part1;            // key[1]
    PartType _part2:48;         // key[2]
    PartType _moveCount:16;     // value
    GameState(const Game& game, unsigned moveCount) noexcept;
    bool operator==(const GameState& other) const noexcept
    {
        return _part0 == other._part0
            && _part1 == other._part1
            && _part2 == other._part2;
    }
};
static_assert(sizeof(GameState) == 24);
struct Hasher
{
    size_t operator() (const GameState & gs) const noexcept
    {
        return 	  gs._part0
                ^ gs._part1
                ^ gs._part2
                ;
    }
};

class GameStateMemory
{
private:
    typedef phmap::parallel_flat_hash_set< 
            GameState, 								// key type
            Hasher,									// hash function
            phmap::priv::hash_default_eq<GameState>,// == function
            phmap::priv::Allocator<GameState >, 
            8U, 									// log2(number of submaps)
            std::mutex								// mutex type
        > MapType;
    MapType _states;

    // Starting minimum capacity for hash map
    const unsigned MinCapacity = 4096*1024;

public:
    GameStateMemory() noexcept;
    // Returns true if no equal Game argument has been presented before
    // to this object or the moveCount argument is lower than that
    // associated with previous calls with equal states.
    bool IsShortPathToState(const Game& game, unsigned moveCount) noexcept;
    // Returns the number of states stored.  
    size_t Size()  noexcept {return _states.size();}
};
}   // namespace KSolveNames