// KSolveAStar.hpp declares a Klondike Solitaire solver function and auxiliaries.
// This solver function uses the A* search algorithm.

// MIT License

// Copyright (c) 2020 Jonathan B. Fry (@JonathanFry3)

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef KSOLVEASTAR_HPP
#define KSOLVEASTAR_HPP

#include "Game.hpp"		// for Game, Card, Pile, Move etc.
namespace KSolveNames {
// Solves the game of Klondike Solitaire for minimum moves if possible.
// Returns a result code and a Moves vector.  The vector contains
// the minimum solution if the code returned is SolvedMinimal. It will contain
// a solution that may not be minimal if the code is Solved.
// Otherwise, it will be empty.
//
// This function uses an unpredictable amount of main memory. You can
// control this behavior to some degree by specifying MoveTreeLimit. 
//
// For some insight into how it works, look up the A* algorithm.

enum KSolveAStarCode {SolvedMinimal, Solved, Impossible, GaveUp};
struct KSolveAStarResult
{
    KSolveAStarCode _code;
    Moves _solution;
    unsigned _branchCount;
    unsigned _moveTreeSize;
    unsigned _finalFringeStackSize;

    KSolveAStarResult(KSolveAStarCode code, 
                const Moves& moves, 
                unsigned branchCount,
                unsigned moveCount,
                unsigned finalFringeStackSize)  noexcept
        : _code(code)
        , _solution(moves)
        , _branchCount(branchCount)
        , _moveTreeSize(moveCount)
        , _finalFringeStackSize(finalFringeStackSize)
        {}
};
KSolveAStarResult KSolveAStar(
        Game& gm, 			// The game to be played
        unsigned MoveTreeLimit=12'000'000,// Give up if the size of the move tree
                                        // exceeds this.
        unsigned threads=0) noexcept;   // Use as many threads as the hardware will run together

unsigned DefaultThreads() noexcept;

unsigned MinimumMovesLeft(const Game& game) noexcept;
}       // namespace KSolveNames


#endif    // KSOLVEASTAR_HPP 