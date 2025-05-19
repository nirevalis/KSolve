/*
	Implements Game.hpp.
*/

#include "Game.hpp"
#include <cassert>
#include <algorithm>		// swap
#include <random>

const std::string suits("cdsh");
const std::string ranks("a23456789tjqk");

namespace KSolveNames {

	// Returns a string composed only of the characters in input that also appear in filter
	static std::string Filtered(std::string input, std::string filter)
	{
		std::string result;
		size_t pos{ 0 };
		while ((pos = input.find_first_of(filter, pos)) != std::string::npos) {
			result += input[pos++];
		}
		return result;
	}

	static std::string LowerCase(const std::string& in)
	{
		std::string result;
		result.reserve(in.length());
		for (auto ch : in) {
			result.push_back(std::tolower(ch));
		}
		return result;
	}

	// Return a string like "d5" or "ca" given a Card
	std::string Card::AsString() const
	{
		return std::string(1, suits[_suit]) + ranks[_rank];
	}

	// Make a Card from a string like "ah" or "s8".
	// Returns true if it succeeds.
	std::optional<Card> CardFromString(const std::string& s0) noexcept
	{
		const std::string s1 = Filtered(LowerCase(s0), suits + ranks + "10");
		Card::SuitT suit;
		std::string rankStr;
		std::optional<Card> result;
		bool ok = s1.length() == 2 || s1.length() == 3;
		if (ok) {
			auto suitIndex = suits.find(s1[0]);
			if (suitIndex != std::string::npos) {
				// input has suit first
				suit = static_cast<Card::SuitT>(suitIndex);
				rankStr = s1.substr(1);
			}
			else {
				// suit does not appear first in input
				{
					// assume it is last
					suitIndex = suits.find(s1.back());
					ok = suitIndex != std::string::npos;
					if (ok) {
						suit = static_cast<Card::SuitT>(suitIndex);
						rankStr = s1.substr(0, s1.length() - 1);
					}
				}
			}
		}
		if (ok) {
			if (rankStr == "10") { rankStr = "t"; }
			auto rankIndex = ranks.find(rankStr);
			if (rankIndex != std::string::npos) {
				Card::RankT rank = static_cast<Card::RankT>(rankIndex);
				result.emplace(suit, rank);
			}
		}
		return result;
	}

	// A reproducible shuffle function.
	// Results from std::shuffle() differ between g++ and MSVC
	void Shuffle(CardDeck& deck, uint32_t seed)
	{
		unsigned n = deck.size();
		if (n < 2) return;

		// Create and seed a random number generator
		std::mt19937 engine;
		engine.seed(seed);


		// Swap each card with a randomly chosen card.
		for (unsigned i = 0; i < n - 2; ++i) {

			// Create a uniform random distribution of integers from i to n-1
			std::uniform_int_distribution<unsigned> distribution(i, n - 1);

			unsigned j = distribution(engine);
			std::swap(deck[i], deck[j]);
		}
	}

	CardDeck NumberedDeal(uint32_t seed)
	{
		// Create a new sorted pack of cards
		CardDeck deck(52);
		std::iota(deck.begin(), deck.end(), 0);
		// Randomly shuffle the deck
		Shuffle(deck, seed);
		return deck;
	}

	Game::Game(CardDeck deck, unsigned draw, unsigned recycleLimit)
		: _deck(deck)
		, _waste(Waste)
		, _stock(Stock)
		, _drawSetting(draw)
		, _recycleLimit(recycleLimit)
		, _tableau{ Tableau1,Tableau2,Tableau3,Tableau4,Tableau5,Tableau6,Tableau7 }
		, _foundation{ Foundation1C,Foundation2D,Foundation3S,Foundation4H }
	{
		Deal();
	}
	Game::Game(const Game& orig)
		: _deck(orig._deck)
		, _waste(orig._waste)
		, _stock(orig._stock)
		, _drawSetting(orig._drawSetting)
		, _recycleLimit(orig._recycleLimit)
		, _recycleCount(orig._recycleCount)
		, _kingSpaces(orig._kingSpaces)
		, _tableau(orig._tableau)
		, _foundation(orig._foundation)
	{
	}

	// Deal the cards for Klondike Solitaire.
	void Game::Deal() noexcept
	{
		assert(_deck.size() == CardsPerDeck);
		_kingSpaces = 0;
		_recycleCount = 0;

		for (auto& pile : AllPiles()) {
			pile.ClearCards();
		}
		// Deal 28 cards to the tableau
		auto iDeck = _deck.cbegin();
		for (unsigned iPile = 0; iPile < TableauSize; iPile += 1) {
			for (unsigned icd = iPile; icd < TableauSize; ++icd)
				_tableau[icd].push_back(*iDeck++);
			_tableau[iPile].SetUpCount(1);      // turn up the top card
			_kingSpaces += _tableau[iPile][0].Rank() == Card::King;	// count kings at base
		}
		// Deal last 24 cards to stock, reversing order
		_stock.assign(_deck.crbegin(), _deck.crbegin() + 24);
	}

	void Game::MakeMove(MoveSpec mv) noexcept
	{
		const auto to = mv.To();
		Pile& toPile = AllPiles()[to];
		if (mv.IsStockMove()) {
			_waste.Draw(_stock, mv.DrawCount());
			toPile.Push(_waste.Pop());
			toPile.IncrUpCount(1);
			_recycleCount += mv.Recycle();
		}
		else {
			const int n = mv.NCards();
			Pile& fromPile = AllPiles()[mv.From()];
			const auto isLadderMove{ mv.IsLadderMove() };
			toPile.Take(fromPile, n);
			assert(!(fromPile.IsTableau() && fromPile.UpCount() != mv.FromUpCount()));
			if (isLadderMove) {
				_foundation[mv.LadderSuit()].Draw(fromPile);
			}
			// For tableau piles, UpCount counts face-up cards.  
			// For other piles, it is undefined.
			toPile.IncrUpCount(n);
			if (fromPile.size()) {
				fromPile.IncrUpCount(mv.FlipsTopCard() - (n + isLadderMove));
			}
			else {
				_kingSpaces += fromPile.IsTableau(); // count newly cleared columns
				fromPile.SetUpCount(0);
			}
		}
	}

	void  Game::UnMakeMove(MoveSpec mv) noexcept
	{
		const auto to = mv.To();
		Pile& toPile = AllPiles()[to];
		if (mv.IsStockMove()) {
			_waste.Push(toPile.Pop());
			toPile.IncrUpCount(-1);
			_stock.Draw(_waste, mv.DrawCount());
			if (mv.Recycle()) --_recycleCount;
		}
		else {
			const auto n = mv.NCards();
			Pile& fromPile = AllPiles()[mv.From()];
			if (mv.IsLadderMove()) {
				// Ladder move
				// UnMakeMove(NonStockMove(fromPile.Code(), 
				//    _foundation[mv.LadderSuit()].Code(),1,mv.FromUpCount()));
				_kingSpaces -= fromPile.empty();
				fromPile.Draw(_foundation[mv.LadderSuit()]);
			}
			if (fromPile.IsTableau()) {
				_kingSpaces -= fromPile.empty();  // uncount newly cleared columns
				fromPile.SetUpCount(mv.FromUpCount());
			}
			fromPile.Take(toPile, n);
			toPile.IncrUpCount(-static_cast<int32_t>(n));
		}
	}

	void Game::MakeMove(const XMove& xmv) noexcept
	{
		const auto from = xmv.From();
		const auto to = xmv.To();
		const unsigned n = xmv.NCards();
		Pile& toPile = AllPiles()[to];
		Pile& fromPile = AllPiles()[from];

		if (from == Stock || to == Stock)
			toPile.Draw(fromPile, n);
		else
			toPile.Take(fromPile, n);
		if (fromPile.empty() && fromPile.IsTableau())
			_kingSpaces += 1;
		toPile.IncrUpCount(n);
		fromPile.IncrUpCount(-static_cast<int32_t>(n));
		if (xmv.Flip()) {
			fromPile.SetUpCount(1);    // flip the top card
		}
	}

	// Return true if all CardsPerDeck cards are in the foundation
	bool Game::GameOver() const noexcept
	{
		return std::all_of(_foundation.cbegin(), _foundation.cend(),
			[&](const auto& pile) {return pile.size() == CardsPerSuit;});
	}

	// Return the height of the shortest foundation pile
	unsigned Game::MinFoundationPileSize() const noexcept
	{
		const auto& fnd = _foundation;
		return std::min_element(fnd.cbegin(), fnd.cend(),
			[&](auto& left, auto& right)
			{return left.size() < right.size();})->size();
	}

	// If there are any available moves from waste, tableau, or
	// top of the stock to a short foundation pile, append one
	// such move to the moves vector.  A short foundation pile
	// here is a foundation pile than which no foundation pile
	// is more than one card shorter.
	//
	// Such a move is called "dominant", meaning that, if the
	// game can be won from this position, no sequence that
	// does not start with such a move can be shorter than
	// the shortest sequences that start with it.
	void Game::DominantAvailableMoves(
		MoveCacheType& moves, unsigned minFoundationSize) const noexcept
	{
		// Loop over Waste, all Tableau piles
		const auto end = AllPiles().begin() + Tableau7;
		for (auto iPile = AllPiles().begin() + Waste; iPile <= end; ++iPile) {
			const Pile& pile = *iPile;
			if (pile.size()) {
				const Card& card = pile.back();
				const auto fromPile = pile.Code();
				if (card.Rank() <= minFoundationSize + 1
					&& CanMoveToFoundation(card)) {
					const auto toPile = FoundationPileCode(card.Suit());
					const unsigned up = (fromPile == Waste) ? 0 : pile.UpCount();
					_domMovesCache.AddNonStockMove(fromPile, toPile, 1, up);
					_domMovesCache.back().FlipsTopCard(pile.IsTableau() && up == 1 && pile.size() > 1);
				}
			}
		}
		if (_drawSetting == 1 && _stock.size()) {
			const Card& card = _stock.back();
			if (card.Rank() <= minFoundationSize + 1 && CanMoveToFoundation(card)) {
				// Stock MoveSpec: draw one card, move it to foundation
				const auto toPile = FoundationPileCode(_stock.back().Suit());
				_domMovesCache.AddStockMove(toPile, 2, 1, false);
			}
		}
	}

	// Append to "moves" any available moves from tableau piles.
	void Game::MovesFromTableau(QMoves& moves) const noexcept
	{
		for (const auto& fromPile : _tableau) {
			// skip empty from piles
			if (fromPile.empty()) continue;

			const Card fromTip = fromPile.back();
			const Card fromBase = fromPile.Top();
			const auto upCount = fromPile.UpCount();

			// look for moves from tableau to foundation
			if (CanMoveToFoundation(fromTip)) {
				const auto toPile = FoundationPileCode(fromTip.Suit());
				moves.AddNonStockMove(fromPile.Code(), toPile, 1, upCount);
				moves.back().FlipsTopCard(upCount == 1 && 1 < fromPile.size());
			}

			// Look for moves between tableau piles.  These may involve
			// moving multiple cards.
			bool kingMoved = false;     // prevents moving the same king twice
			for (const auto& toPile : _tableau) {
				if (&fromPile == &toPile) continue;

				if (toPile.empty()) {
					if (!kingMoved
						&& fromBase.Rank() == Card::King
						&& fromPile.size() > upCount) {
						// toPile is empty, a king sits abottom fromPile's face-up
						// cards, and it is covering at least one face-down card.
						moves.AddNonStockMove(fromPile.Code(), toPile.Code(), upCount, upCount);
						moves.back().FlipsTopCard(true);
						kingMoved = true;
					}
				}
				else {
					// Other moves follow the opposite-color-and-next-lower-rank rule.
					// We move from one tableau pile to another only to 
					// (a) move all the face-up cards on the from pile in order to 
					//		(1) flip a face-down card, or
					//		(2) make a useful empty column, or
					// (b) uncover a face-up card on the from pile that can be moved
					// 	   to a foundation pile.
					const Card cardToCover = toPile.back();
					// See whether any of the from pile's up cards can be moved
					// to the to pile.
					const unsigned toRank = cardToCover.Rank();
					if (fromTip.Rank() < toRank && toRank <= fromBase.Rank() + 1U
						&& (fromTip.OddRed() == cardToCover.OddRed())) {
						// Some face-up card in the from pile covers the top card
						// in the to pile, so a move is possible.
						const unsigned moveCount = cardToCover.Rank() - fromTip.Rank();
						assert(moveCount <= upCount);
						if (moveCount == upCount && (upCount < fromPile.size() || NeedKingSpace())) {
							// This move will flip a face-down card or
							// clear a column that's needed for a king.
							// Move all the face-up cards on the from pile.
							assert(fromBase.Covers(cardToCover));
							moves.AddNonStockMove(fromPile.Code(), toPile.Code(), upCount, upCount);
							moves.back().FlipsTopCard(upCount < fromPile.size());
						}
						else if (moveCount < upCount || upCount < fromPile.size()) {
							const Card uncovered = *(fromPile.end() - moveCount - 1);
							if (CanMoveToFoundation(uncovered)) {
								// This move will uncover a card that can be moved to 
								// its foundation pile and move it there.
								assert((fromPile.end() - moveCount)->Covers(cardToCover));
								moves.AddLadderMove(fromPile.Code(), toPile.Code(), moveCount,
									upCount, uncovered);
								moves.back().FlipsTopCard(upCount == moveCount + 1);
							}
						}
					}
				}
			}
		}
	}

	struct TalonFuture
	{
		Card _card;
		unsigned short _nMoves;
		signed short _drawCount;
		bool _recycle;

		TalonFuture() {};
		TalonFuture(const Card& card, unsigned nMoves, int draw, bool recycle)
			: _card(card)
			, _nMoves(nMoves)
			, _drawCount(draw)
			, _recycle(recycle)
		{
		}
	};

	// Class to simulate draws and recycles of the talon and
	// return the top card of the simulated waste pile.
	class TalonSim
	{
		const PileVec& _waste;
		const PileVec& _stock;
		unsigned _wSize;
		unsigned _sSize;
	public:
		TalonSim(const Game& game)
			: _waste(game.WastePile())
			, _stock(game.StockPile())
			, _wSize(game.WastePile().size())
			, _sSize(game.StockPile().size())
		{
		}
		unsigned WasteSize() const
		{
			return _wSize;
		}
		unsigned StockSize() const
		{
			return _sSize;
		}
		void Cycle()
		{
			_sSize += _wSize;
			_wSize = 0;
		}
		void Draw(unsigned n)
		{
			n = std::min<unsigned>(n, _sSize);
			_wSize += n;
			_sSize -= n;
		}
		Card TopCard() const
		{
			if (_wSize <= _waste.size()) {
				return _waste[_wSize - 1];
			}
			else {
				return *(_stock.end() - (_wSize - _waste.size()));
			}
		}
	};

	// Return a vector of all the cards that can be played
	// from the talon (the stock and waste piles), along
	// with the number of moves required to reach each one
	// and the number of cards that must be drawn (or undrawn)
	// to reach each one.
	//
	// Enforces the limit on recycles
	typedef static_vector<TalonFuture, 24> TalonFutureVec;
	static TalonFutureVec TalonCards(const Game& game)
	{
		TalonFutureVec result;
		const unsigned talonSize = game.WastePile().size() + game.StockPile().size();
		if (talonSize == 0) return result;

		TalonSim talon(game);
		const unsigned originalWasteSize = talon.WasteSize();
		const unsigned drawSetting = game.DrawSetting();
		unsigned nMoves = 0;
		unsigned nRecycles = 0;
		unsigned maxRecycles = std::min(1U, game.RecycleLimit() - game.RecycleCount());

		do {
			if (talon.WasteSize()) {
				result.emplace_back(talon.TopCard(), nMoves,
					talon.WasteSize() - originalWasteSize, nRecycles > 0);
			}
			if (talon.StockSize()) {
				// Draw from the stock pile
				nMoves += 1;
				talon.Draw(drawSetting);
			}
			else {
				// Recycle the waste pile
				nRecycles += 1;
				talon.Cycle();
			}
		} while (talon.WasteSize() != originalWasteSize && nRecycles <= maxRecycles);
		return result;
	}

	// Append to "moves" any available moves from the talon.
	void Game::MovesFromTalon(QMoves& moves, unsigned minFoundationSize) const noexcept
	{
		// Look for move from the talon to tableau or foundation, including moves that become available 
		// after one or more draws.  
		const TalonFutureVec talon(TalonCards(*this));
		for (auto talonCard : talon) {
			bool recycle = talonCard._recycle;
			if (CanMoveToFoundation(talonCard._card)) {
				const auto pileNo = FoundationPileCode(talonCard._card.Suit());
				moves.AddStockMove(pileNo, talonCard._nMoves + 1, talonCard._drawCount, recycle);
				if (talonCard._card.Rank() <= minFoundationSize + 1) {
					if (_drawSetting == 1) {
						break;		// This is best next move from among the remaining talon cards
					}
					else
						continue;  	// This is best move for this card.  A card further on might be a better move.
				}
			}

			for (const auto& tPile : _tableau) {
				if ((tPile.size() > 0)) {
					if (talonCard._card.Covers(tPile.back())) {
						moves.AddStockMove(tPile.Code(), talonCard._nMoves + 1,
							talonCard._drawCount, recycle);
					}
				}
				else if (talonCard._card.Rank() == Card::King) {
					moves.AddStockMove(tPile.Code(), talonCard._nMoves + 1,
						talonCard._drawCount, recycle);
					break;  // move that king to just one empty pile
				}
			}
		}
	}

	// Look for moves from foundation piles to tableau piles.
	void Game::MovesFromFoundation(QMoves& moves, unsigned minFoundationSize) const noexcept
	{
		for (const auto& fPile : _foundation) {
			// Avoid generating moves whose reversals are dominant.
			if (fPile.size() <= minFoundationSize + 2) continue;
			const Card& top = fPile.back();
			for (const auto& tPile : _tableau) {
				if (tPile.size() > 0) {
					if (top.Covers(tPile.back())) {
						moves.AddNonStockMove(fPile.Code(), tPile.Code(), 1, 0);
					}
				}
				else {
					if (top.Rank() == Card::King) {
						moves.AddNonStockMove(fPile.Code(), tPile.Code(), 1, 0);
						break;  // don't move same king to another tableau pile
					}
				}
			}
		}
	}

	// Returns a list of valid non-dominant moves - they either do not move
	// a card to the foundation or they unbalance the foundation.
	// Rather than generate individual draws from
	// stock to waste, it generates Move objects that represent one or more
	// draws and that expose a playable top waste card and then play that card.
	void Game::NonDominantAvailableMoves(QMoves& moves, unsigned minFoundationSize) const noexcept
	{
		MovesFromTableau(moves);
		MovesFromTalon(moves, minFoundationSize);
		MovesFromFoundation(moves, minFoundationSize);
		return;
	}

	static bool Valid(const Game& gm,
		unsigned from,
		unsigned to,
		unsigned nCardsToMove)
	{
		if (from >= PileCount) return false;
		if (to >= PileCount) return false;
		if (nCardsToMove == 0 || nCardsToMove > 24) return false;
		const Pile& fromPile = gm.AllPiles()[from];
		const Pile& toPile = gm.AllPiles()[to];
		if (nCardsToMove > fromPile.size()) return false;
		Card coverCard = *(fromPile.end() - nCardsToMove);
		if (toPile.IsTableau()) {
			if (toPile.empty()) {
				if (coverCard.Rank() != Card::King) return false;
			}
			else {
				if (!coverCard.Covers(toPile.back())) return false;
			}
		}
		else if (toPile.IsFoundation()) {
			if (coverCard.Suit() != to - FoundationBase) return false;
			if (coverCard.Rank() != toPile.size()) return false;
		}
		return true;
	}

	bool Game::IsValid(MoveSpec mv) const noexcept
	{
		if (mv.IsStockMove()) {
			int draw = mv.DrawCount();
			if (draw > 0) {
				return Valid(*this, Stock, mv.To(), draw);
			}
			else {
				return Valid(*this, Waste, mv.To(), -draw + 1);
			}
		}
		else
			return Valid(*this, mv.From(), mv.To(), mv.NCards());
	}

	bool Game::IsValid(XMove mv) const noexcept
	{
		return Valid(*this, mv.From(), mv.To(), mv.NCards());
	}

	// Enumerate the moves in a vector of MoveSpecs.
	std::vector<XMove> MakeXMoves(const Moves& solution, unsigned draw)
	{
		unsigned stockSize = 24;
		unsigned wasteSize = 0;
		unsigned mvnum = 0;
		std::vector<XMove> result;

		for (auto mv : solution) {
			const auto from = mv.From();
			const auto to = mv.To();

			if (!mv.IsStockMove()) {
				unsigned n = mv.NCards();
				bool flip = mv.FlipsTopCard() && !mv.IsLadderMove();
				mvnum++;
				result.emplace_back(mvnum, from, to, n, flip);
				if (mv.From() == Waste) {
					assert(wasteSize >= 1);
					wasteSize--;
				}
				if (mv.IsLadderMove()) {
					// Ladder move. Generate the extra move to foundation
					flip = mv.FlipsTopCard();
					++mvnum;
					const auto ladderPile = mv.LadderPileCode();
					result.emplace_back(mvnum, from, ladderPile, 1, flip);
				}
			}
			else {
				assert(stockSize + wasteSize > 0);
				unsigned nTalonMoves = mv.NMoves() - 1;
				const unsigned stockMovesLeft = QuotientRoundedUp(stockSize, draw);
				if (nTalonMoves > stockMovesLeft && stockSize) {
					// Draw all remaining cards from stock
					mvnum++;
					result.emplace_back(mvnum, Stock, Waste, stockSize, false);
					mvnum += stockMovesLeft - 1;
					wasteSize += stockSize;
					stockSize = 0;
					nTalonMoves -= stockMovesLeft;
				}
				if (nTalonMoves > 0) {
					mvnum += 1;
					if (stockSize == 0) {
						// Recycle the waste pile
						result.emplace_back(mvnum, Waste, Stock, wasteSize, false);
						stockSize = wasteSize;
						wasteSize = 0;
					}
					const unsigned nMoved = std::min<unsigned>(stockSize, nTalonMoves * draw);
					result.emplace_back(mvnum, Stock, Waste, nMoved, false);
					assert(stockSize >= nMoved && wasteSize + nMoved <= 24);
					assert(stockSize >= nMoved);
					stockSize -= nMoved;
					wasteSize += nMoved;
					assert(wasteSize <= 24);
					mvnum += nTalonMoves - 1;
				}
				mvnum++;
				result.emplace_back(mvnum, Waste, to, 1, false);
				assert(wasteSize >= 1);
				wasteSize--;
			}
		}
		return result;
	}

	static std::string PileNames[]
	{
		"wa",
		"t1",
		"t2",
		"t3",
		"t4",
		"t5",
		"t6",
		"t7",
		"st",
		"cb",
		"di",
		"sp",
		"ht"
	};

	std::string Peek(const Pile& pile)
	{
		std::stringstream outStr;
		outStr << PileNames[pile.Code()] << ":";
		for (auto ip = pile.begin(); ip < pile.end(); ++ip)
		{
			char sep(' ');
			if (pile.IsTableau())
			{
				if (ip == pile.end() - pile.UpCount()) { sep = '|'; }
			}
			outStr << sep << ip->AsString();
		}
		return outStr.str();
	}

	std::string Peek(const MoveSpec& mv)
	{
		std::stringstream outStr;
		if (mv.IsStockMove()) {
			outStr << "+" << mv.NMoves() << "d" << mv.DrawCount();
			if (mv.Recycle()) {
				outStr << "c";
			}
			outStr << ">" << PileNames[mv.To()];
		}
		else {
			outStr << PileNames[mv.From()] << ">" << PileNames[mv.To()];
			unsigned n = mv.NCards();
			if (n != 1) outStr << "x" << n;
			if (mv.FromUpCount()) outStr << "u" << mv.FromUpCount();
		}
		return outStr.str();
	}

	std::string Peek(const Game& game)
	{
		std::stringstream out;
		const auto& piles = game.AllPiles();
		for (const auto& pile : piles) {
			out << Peek(pile) << "\n";
		}
		return out.str();
	}

}   // namespace KSolveNames