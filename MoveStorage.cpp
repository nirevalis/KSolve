#include "MoveStorage.hpp"
#include <iostream>

namespace KSolveNames {

void SharedMoveStorage::Start(size_t moveTreeSizeLimit, unsigned minMoves) noexcept
{
    _moveTreeSizeLimit = moveTreeSizeLimit;
    _moveTree.reserve(moveTreeSizeLimit+1000);
    _initialMinMoves = minMoves;
    _firstTime = true;
}
MoveStorage::MoveStorage(SharedMoveStorage& shared) noexcept
    : _shared(shared)
    , _startSize(0)
    {}
void MoveStorage::PushStem(MoveSpec move) noexcept
{
    // This is where the program fails when XYZ_Test give false negatives.
    if (!(_currentSequence.size() < _currentSequence.capacity())) {
        std::string movesString = Peek(_currentSequence);
        std::cerr << movesString << std::endl;
        assert(false && "XYZ_Test false negatives");
    }
    _currentSequence.push_back(move);
}
void MoveStorage::PushBranch(MoveSpec mv, unsigned nMoves) noexcept
{
    assert(_shared._initialMinMoves <= nMoves);
    _branches.emplace_back(mv,nMoves-_shared._initialMinMoves);
}
void MoveStorage::ShareMoves() noexcept
{
    // If _branches is empty, a dead end has been reached.  There
    // is no need to store any stem nodes that led to it.
    if (_branches.size()) {
        NodeX stemEnd      // index in _moveTree of last stem MoveNode
            = UpdateMoveTree();
        UpdateFringe(stemEnd);
        _branches.clear();
    }
}
// Returns move tree index of last stem node
NodeX MoveStorage::UpdateMoveTree() noexcept
{
    NodeX stemEnd = _leaf._prevNode;
    {
        Guard rupert(_shared._moveTreeMutex);
        // Copy all the stem moves into the move tree.
        for (auto m: _currentSequence | views::drop(_startSize))
        {
            // Each stem node points to the previous node.
            _shared._moveTree.emplace_back(m, stemEnd);
            stemEnd =  _shared._moveTree.size() - 1;
        }
    }
    return stemEnd;
} 
void MoveStorage::UpdateFringe(NodeX stemEnd) noexcept
{
    ranges::sort(_branches,ranges::greater(),&MovePair::_offset);  // descending by offset
    auto & fringe = _shared._fringe;
    for (const auto &br: _branches) {
        fringe.Emplace(br._offset, br._mv, stemEnd);
    }
}
unsigned MoveStorage::PopNextMoveSequence( ) noexcept
{
    if (_shared._firstTime) {
        _shared._firstTime = false;
        return _shared._initialMinMoves;
    }
    auto nextLeaf = _shared._fringe.Pop();
    if (nextLeaf) {
        _leaf = nextLeaf->second;
        return nextLeaf->first+_shared._initialMinMoves;
    } else {
        return 0;     // last time for this thread
    }
}
void MoveStorage::LoadMoveSequence() noexcept
{
    // Follow the links to recover all the moves in a sequence in reverse order.
    _currentSequence.clear();
    for    (NodeX node = _leaf._prevNode; 
            node != -1U; 
            node = _shared._moveTree[node]._prevNode){
        const MoveSpec &mv = _shared._moveTree[node]._move;
        _currentSequence.push_front(mv);
    }
    _startSize = _currentSequence.size();
    if (!_leaf._move.IsDefault()) 
        _currentSequence.push_back(_leaf._move);
}
void MoveStorage::MakeSequenceMoves(Game&game) const noexcept
{
    for (auto & move: _currentSequence){
        game.MakeMove(move);
    }
}
}   // namespace KSolveNames
