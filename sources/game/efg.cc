//
// $Source$
// $Date$
// $Revision$
//
// DESCRIPTION:
// Implementation of extensive form game representation
//
// This file is part of Gambit
// Copyright (c) 2002, The Gambit Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//

#include "base/base.h"
#include "math/rational.h"

#include "efg.h"
#include "efgutils.h"
#include "efstrat.h"
#include "actiter.h"

// Declarations of internal structures
#include "efgint.h"

//------------------------------------------------------------------------
//       Efg: Constructors, destructor, constructive operators
//------------------------------------------------------------------------

efgGame::efgGame(void)
  : sortisets(true), m_dirty(false), m_revision(0), 
    m_outcome_revision(-1), title("UNTITLED"),
    chance(new gbt_efg_player_rep(this, 0)), afg(0), lexicon(0)
{
  root = new Node(this, 0);

  SortInfosets();
}

efgGame::efgGame(const efgGame &E, Node *n /* = 0 */)
  : sortisets(false), m_dirty(false), m_revision(0), 
    m_outcome_revision(-1), title(E.title), comment(E.comment),
    players(E.players.Length()), outcomes(0, E.outcomes.Last()),
    chance(new gbt_efg_player_rep(this, 0)),
    afg(0), lexicon(0) 
{
  for (int i = 1; i <= players.Length(); i++)  {
    (players[i] = new gbt_efg_player_rep(this, i))->m_label = E.players[i]->m_label;
    for (int j = 1; j <= E.players[i]->m_infosets.Length(); j++)   {
      gbt_efg_infoset_rep *s = new gbt_efg_infoset_rep(players[i], j,
						       E.players[i]->m_infosets[j]->m_actions.Length());
      s->m_label = E.players[i]->m_infosets[j]->m_label;
      for (int k = 1; k <= s->m_actions.Length(); k++) {
	s->m_actions[k]->m_label = E.players[i]->m_infosets[j]->m_actions[k]->m_label;
      }
      players[i]->m_infosets.Append(s);
    }
  }

  for (int i = 1; i <= E.GetChance().NumInfosets(); i++)   {
    gbt_efg_infoset_rep *t = E.chance->m_infosets[i];
    gbt_efg_infoset_rep *s = new gbt_efg_infoset_rep(chance, i, t->m_actions.Length());
    s->m_label = t->m_label;
    for (int act = 1; act <= s->m_chanceProbs.Length(); act++) {
      s->m_chanceProbs[act] = t->m_chanceProbs[act];
      s->m_actions[act]->m_label = t->m_actions[act]->m_label;
    }
    chance->m_infosets.Append(s);
  }

  for (int outc = 1; outc <= E.NumOutcomes(); outc++)  {
    outcomes[outc] = new gbt_efg_outcome_rep(this, outc);
    outcomes[outc]->m_label = E.outcomes[outc]->m_label;
    outcomes[outc]->m_payoffs = E.outcomes[outc]->m_payoffs;
  }

  root = new Node(this, 0);
  CopySubtree(root, (n ? n : E.RootNode()));
  
  if (n)   {
    for (int pl = 1; pl <= players.Length(); pl++)  {
      for (int i = 1; i <= players[pl]->m_infosets.Length(); i++)  {
	if (players[pl]->m_infosets[i]->m_members.Length() == 0)
	  delete players[pl]->m_infosets.Remove(i--);
      }
    }
  }

  sortisets = true;
  SortInfosets();
}

#include "lexicon.h"

efgGame::~efgGame()
{
  delete root;
  delete chance;

  for (int i = 1; i <= players.Length(); delete players[i++]);
  for (int i = 1; i <= outcomes.Last(); delete outcomes[i++]);

  if (lexicon)   delete lexicon;
  lexicon = 0;
}

//------------------------------------------------------------------------
//                  Efg: Private member functions
//------------------------------------------------------------------------

void efgGame::DeleteLexicon(void) const
{
  if (lexicon)   delete lexicon;
  lexicon = 0;
}

gbtEfgInfoset efgGame::GetInfosetByIndex(gbtEfgPlayer p, int index) const
{
  for (int i = 1; i <= p.rep->m_infosets.Length(); i++) {
    if (p.rep->m_infosets[i]->m_id == index) {
      return p.rep->m_infosets[i];
    }
  }
  return 0;
}

gbtEfgOutcome efgGame::GetOutcomeByIndex(int index) const
{
  for (int i = 1; i <= outcomes.Last(); i++) {
    if (outcomes[i]->m_id == index)  {
      return outcomes[i];
    }
  }

  return 0;
}

void efgGame::Reindex(void)
{
  int i;

  for (i = 1; i <= players.Length(); i++)  {
    gbt_efg_player_rep *p = players[i];
    for (int j = 1; j <= p->m_infosets.Length(); j++) {
      p->m_infosets[j]->m_id = j;
    }
  }

  for (i = 1; i <= outcomes.Last(); i++) {
    outcomes[i]->m_id = i;
  }
}


void efgGame::NumberNodes(Node *n, int &index)
{
  n->number = index++;
  for (int child = 1; child <= n->children.Length();
       NumberNodes(n->children[child++], index));
} 

void efgGame::SortInfosets(void)
{
  if (!sortisets)  return;

  int pl;

  for (pl = 0; pl <= players.Length(); pl++)  {
    gList<Node *> nodes;

    Nodes(*this, nodes);

    gbt_efg_player_rep *player = (pl) ? players[pl] : chance;

    int i, isets = 0;

    // First, move all empty infosets to the back of the list so
    // we don't "lose" them
    int foo = player->m_infosets.Length();
    i = 1;
    while (i < foo)   {
      if (player->m_infosets[i]->m_members.Length() == 0)  {
	gbt_efg_infoset_rep *bar = player->m_infosets[i];
	player->m_infosets[i] = player->m_infosets[foo];
	player->m_infosets[foo--] = bar;
      }
      else
	i++;
    }

    // This will give empty infosets their proper number; the nonempty
    // ones will be renumbered by the next loop
    for (i = 1; i <= player->m_infosets.Length(); i++)
      if (player->m_infosets[i]->m_members.Length() == 0)
	player->m_infosets[i]->m_id = i;
      else
	player->m_infosets[i]->m_id = 0;
  
    for (i = 1; i <= nodes.Length(); i++)  {
      Node *n = nodes[i];
      if (n->GetPlayer() == player && n->infoset->m_id == 0)  {
	n->infoset->m_id = ++isets;
	player->m_infosets[isets] = n->infoset;
      }
    }  
  }

  // Now, we sort the nodes within the infosets
  
  gList<Node *> nodes;
  Nodes(*this, nodes);

  for (pl = 0; pl <= players.Length(); pl++)  {
    gbt_efg_player_rep *player = (pl) ? players[pl] : chance;

    for (int iset = 1; iset <= player->m_infosets.Length(); iset++)  {
      gbt_efg_infoset_rep *s = player->m_infosets[iset];
      for (int i = 1, j = 1; i <= nodes.Length(); i++)  {
	if (nodes[i]->infoset == s)
	  s->m_members[j++] = nodes[i];
      }
    }
  }

  int nodeindex = 1;
  NumberNodes(root, nodeindex);
}
  
gbt_efg_infoset_rep *efgGame::CreateInfoset(int n, gbtEfgPlayer p, int br)
{
  gbt_efg_infoset_rep *s = new gbt_efg_infoset_rep(p.rep, n, br);
  p.rep->m_infosets.Append(s);
  return s;
}

gbtEfgOutcome efgGame::CreateOutcomeByIndex(int index)
{
  NewOutcome(index);
  return outcomes[outcomes.Last()];
}

void efgGame::CopySubtree(Node *n, Node *m)
{
  n->name = m->name;

  if (m->gameroot == m)
    n->gameroot = n;

  if (m->outcome) {
    n->outcome = m->outcome;
  }

  if (m->infoset)   {
    gbt_efg_player_rep *p;
    if (m->infoset->m_player->m_id) {
      p = players[m->infoset->m_player->m_id];
    }
    else {
      p = chance;
    }

    gbt_efg_infoset_rep *s = p->m_infosets[m->infoset->m_id];
    AppendNode(n, s);

    for (int i = 1; i <= n->children.Length(); i++)
      CopySubtree(n->children[i], m->children[i]);
  }
}

//------------------------------------------------------------------------
//               Efg: Title access and manipulation
//------------------------------------------------------------------------

void efgGame::SetTitle(const gText &s)
{
  title = s; 
  m_revision++;
  m_dirty = true;
}

const gText &efgGame::GetTitle(void) const
{ return title; }

void efgGame::SetComment(const gText &s)
{
  comment = s;
  m_revision++;
  m_dirty = true;
}

const gText &efgGame::GetComment(void) const
{ return comment; }
  

//------------------------------------------------------------------------
//                    Efg: Writing data files
//------------------------------------------------------------------------

void efgGame::WriteEfgFile(gOutput &f, Node *n) const
{
  if (n->children.Length() == 0)   {
    f << "t \"" << EscapeQuotes(n->name) << "\" ";
    if (n->outcome)  {
      f << n->outcome->m_id << " \"" <<
	EscapeQuotes(n->outcome->m_label) << "\" ";
      f << "{ ";
      for (int pl = 1; pl <= NumPlayers(); pl++)  {
	f << n->outcome->m_payoffs[pl];
	if (pl < NumPlayers())
	  f << ", ";
	else
	  f << " }\n";
      }
    }
    else
      f << "0\n";
  }

  else if (n->infoset->m_player->m_id)   {
    f << "p \"" << EscapeQuotes(n->name) << "\" " <<
      n->infoset->m_player->m_id << ' ';
    f << n->infoset->m_id << " \"" <<
      EscapeQuotes(n->infoset->m_label) << "\" ";
    n->infoset->PrintActions(f);
    f << " ";
    if (n->outcome)  {
      f << n->outcome->m_id << " \"" <<
	EscapeQuotes(n->outcome->m_label) << "\" ";
      f << "{ ";
      for (int pl = 1; pl <= NumPlayers(); pl++)  {
	f << n->outcome->m_payoffs[pl];
	if (pl < NumPlayers())
	  f << ", ";
	else
	  f << " }\n";
      }
    }
    else
      f << "0\n";
  }

  else   {    // chance node
    f << "c \"" << n->name << "\" ";
    f << n->infoset->m_id << " \"" <<
      EscapeQuotes(n->infoset->m_label) << "\" ";
    n->infoset->PrintActions(f);
    f << " ";
    if (n->outcome)  {
      f << n->outcome->m_id << " \"" <<
	EscapeQuotes(n->outcome->m_label) << "\" ";
      f << "{ ";
      for (int pl = 1; pl <= NumPlayers(); pl++)  {
	f << n->outcome->m_payoffs[pl];
        if (pl < NumPlayers()) 
          f << ", ";
        else
          f << " }\n";
      }
    }
    else
      f << "0\n";
  }

  for (int i = 1; i <= n->children.Length(); i++)
    WriteEfgFile(f, n->children[i]);
}

void efgGame::WriteEfgFile(gOutput &p_file, int p_nDecimals) const
{
  int oldPrecision = p_file.GetPrec();
  p_file.SetPrec(p_nDecimals);

  try {
    p_file << "EFG 2 R";
    p_file << " \"" << EscapeQuotes(title) << "\" { ";
    for (int i = 1; i <= players.Length(); i++) {
      p_file << '"' << EscapeQuotes(players[i]->m_label) << "\" ";
    }
    p_file << "}\n";
    p_file << "\"" << EscapeQuotes(comment) << "\"\n\n";

    WriteEfgFile(p_file, root);
    p_file.SetPrec(oldPrecision);
    m_revision++;
    m_dirty = false;
  }
  catch (...) {
    p_file.SetPrec(oldPrecision);
    throw;
  }
}


//------------------------------------------------------------------------
//                    Efg: General data access
//------------------------------------------------------------------------

int efgGame::NumPlayers(void) const
{ return players.Length(); }

gbtEfgPlayer efgGame::NewPlayer(void)
{
  m_revision++;
  m_dirty = true;

  gbt_efg_player_rep *ret = new gbt_efg_player_rep(this, players.Length() + 1);
  players.Append(ret);

  for (int outc = 1; outc <= outcomes.Last();
       outcomes[outc++]->m_payoffs.Append(0));
  for (int outc = 1; outc <= outcomes.Last();
       outcomes[outc++]->m_doublePayoffs.Append(0));
  DeleteLexicon();
  return ret;
}

gbtEfgPlayer efgGame::GetPlayer(int p_player) const
{
  if (p_player == 0) {
    return chance;
  }
  else {
    return players[p_player];
  }
}

int efgGame::NumOutcomes(void) const
{ return outcomes.Last(); }

gbtEfgOutcome efgGame::NewOutcome(void)
{
  m_revision++;
  m_dirty = true;
  return NewOutcome(outcomes.Last() + 1);
}

void efgGame::DeleteOutcome(gbtEfgOutcome &p_outcome)
{
  m_revision++;
  m_dirty = true;

  root->DeleteOutcome(p_outcome.rep);
  delete outcomes.Remove(outcomes.Find(p_outcome.rep));
  DeleteLexicon();
}

gbtEfgOutcome efgGame::GetOutcome(int p_index) const
{
  return outcomes[p_index];
}

void efgGame::SetPayoff(gbtEfgOutcome p_outcome, int pl, 
			const gNumber &value)
{
  if (p_outcome.IsNull()) {
    return;
  }

  m_revision++;
  m_dirty = true;
  p_outcome.rep->m_payoffs[pl] = value;
  p_outcome.rep->m_doublePayoffs[pl] = (double) value;
}

gNumber efgGame::Payoff(const gbtEfgOutcome &p_outcome,
			const gbtEfgPlayer &p_player) const
{
  if (p_outcome.IsNull() || p_player.IsNull()) {
    return gNumber(0);
  }

  return p_outcome.rep->m_payoffs[p_player.rep->m_id];
}

gNumber efgGame::Payoff(const Node *p_node, const gbtEfgPlayer &p_player) const
{
  if (p_player.IsNull()) {
    return gNumber(0);
  }

  return ((p_node->outcome) ? 
	  p_node->outcome->m_payoffs[p_player.rep->m_id] : gNumber(0));
}

gArray<gNumber> efgGame::Payoff(const gbtEfgOutcome &p_outcome) const
{
  if (p_outcome.IsNull()) {
    gArray<gNumber> ret(players.Length());
    for (int i = 1; i <= ret.Length(); ret[i++] = 0);
    return ret;
  }
  else {
    return p_outcome.rep->m_payoffs;
  }
}

bool efgGame::IsConstSum(void) const
{
  int pl, index;
  gNumber cvalue = (gNumber) 0;

  if (outcomes.Last() == 0)  return true;

  for (pl = 1; pl <= players.Length(); pl++)
    cvalue += outcomes[1]->m_payoffs[pl];

  for (index = 2; index <= outcomes.Last(); index++)  {
    gNumber thisvalue(0);

    for (pl = 1; pl <= players.Length(); pl++)
      thisvalue += outcomes[index]->m_payoffs[pl];

    if (thisvalue > cvalue || thisvalue < cvalue)
      return false;
  }

  return true;
}

gNumber efgGame::MinPayoff(int pl) const
{
  int index, p, p1, p2;
  gNumber minpay;

  if (NumOutcomes() == 0)  return 0;

  if(pl) { p1=p2=pl;}
  else {p1=1;p2=players.Length();}

  minpay = outcomes[1]->m_payoffs[p1];

  for (index = 1; index <= outcomes.Last(); index++)  {
    for (p = p1; p <= p2; p++)
      if (outcomes[index]->m_payoffs[p] < minpay)
	minpay = outcomes[index]->m_payoffs[p];
  }
  return minpay;
}

gNumber efgGame::MaxPayoff(int pl) const
{
  int index, p, p1, p2;
  gNumber maxpay;

  if (NumOutcomes() == 0)  return 0;

  if(pl) { p1=p2=pl;}
  else {p1=1;p2=players.Length();}

  maxpay = outcomes[1]->m_payoffs[p1];

  for (index = 1; index <= outcomes.Last(); index++)  {
    for (p = p1; p <= p2; p++)
      if (outcomes[index]->m_payoffs[p] > maxpay)
	maxpay = outcomes[index]->m_payoffs[p];
  }
  return maxpay;
}

Node *efgGame::RootNode(void) const
{ return root; }

bool efgGame::IsSuccessor(const Node *n, const Node *from) const
{ return IsPredecessor(from, n); }

bool efgGame::IsPredecessor(const Node *n, const Node *of) const
{
  while (of && n != of)    of = of->parent;

  return (n == of);
}

gbtEfgOutcome efgGame::NewOutcome(int index)
{
  m_revision++;
  m_dirty = true;
  outcomes.Append(new gbt_efg_outcome_rep(this, index));
  return outcomes[outcomes.Last()];
} 

//------------------------------------------------------------------------
//                     Efg: Operations on players
//------------------------------------------------------------------------

gbtEfgPlayer efgGame::GetChance(void) const
{
  return chance;
}

gbtEfgInfoset efgGame::AppendNode(Node *n, gbtEfgPlayer p, int count)
{
  if (!n || p.IsNull() || count == 0)
    throw Exception();

  m_revision++;
  m_dirty = true;

  if (n->children.Length() == 0)   {
    n->infoset = CreateInfoset(p.rep->m_infosets.Length() + 1, p.rep, count);
    n->infoset->m_members.Append(n);
    while (count--)
      n->children.Append(new Node(this, n));
  }

  DeleteLexicon();
  SortInfosets();
  return n->infoset;
}  

gbtEfgInfoset efgGame::AppendNode(Node *n, gbtEfgInfoset s)
{
  if (!n || s.IsNull())   throw Exception();
  
  // Can't bridge subgames...
  if (s.rep->m_members.Length() > 0 && n->gameroot != s.rep->m_members[1]->gameroot)
    return 0;

  if (n->children.Length() == 0)   {
    m_revision++;
    m_dirty = true;
    n->infoset = s.rep;
    s.rep->m_members.Append(n);
    for (int i = 1; i <= s.rep->m_actions.Length(); i++)
      n->children.Append(new Node(this, n));
  }

  DeleteLexicon();
  SortInfosets();
  return s;
}
  
Node *efgGame::DeleteNode(Node *n, Node *keep)
{
  if (!n || !keep)   throw Exception();

  if (keep->parent != n)   return n;

  if (n->gameroot == n)
    MarkSubgame(keep, keep);

  m_revision++;
  m_dirty = true;
  // turn infoset sorting off during tree deletion -- problems will occur
  sortisets = false;

  n->children.Remove(n->children.Find(keep));
  DeleteTree(n);
  keep->parent = n->parent;
  if (n->parent)
    n->parent->children[n->parent->children.Find(n)] = keep;
  else
    root = keep;

  delete n;
  DeleteLexicon();

  sortisets = true;

  SortInfosets();
  return keep;
}

gbtEfgInfoset efgGame::InsertNode(Node *n, gbtEfgPlayer p, int count)
{
  if (!n || p.IsNull() || count <= 0)  {
    throw Exception();
  }

  m_revision++;
  m_dirty = true;

  Node *m = new Node(this, n->parent);
  m->infoset = CreateInfoset(p.rep->m_infosets.Length() + 1, p, count);
  m->infoset->m_members.Append(m);
  if (n->parent)
    n->parent->children[n->parent->children.Find(n)] = m;
  else
    root = m;
  m->children.Append(n);
  n->parent = m;
  while (--count)
    m->children.Append(new Node(this, m));

  DeleteLexicon();
  SortInfosets();
  return m->infoset;
}

gbtEfgInfoset efgGame::InsertNode(Node *n, gbtEfgInfoset s)
{
  if (!n || s.IsNull())  throw Exception();

  // can't bridge subgames
  if (s.rep->m_members.Length() > 0 && n->gameroot != s.rep->m_members[1]->gameroot)
    return 0;
  
  m_revision++;
  m_dirty = true;

  Node *m = new Node(this, n->parent);
  m->infoset = s.rep;
  s.rep->m_members.Append(m);
  if (n->parent)
    n->parent->children[n->parent->children.Find(n)] = m;
  else
    root = m;
  m->children.Append(n);
  n->parent = m;
  int count = s.rep->m_actions.Length();
  while (--count)
    m->children.Append(new Node(this, m));

  DeleteLexicon();
  SortInfosets();
  return m->infoset;
}

gbtEfgInfoset efgGame::CreateInfoset(gbtEfgPlayer p, int br)
{
  if (p.IsNull() || p.GetGame() != this) {
    throw Exception();
  }
  m_revision++;
  m_dirty = true;
  return CreateInfoset(p.rep->m_infosets.Length() + 1, p.rep, br);
}

gbtEfgInfoset efgGame::JoinInfoset(gbtEfgInfoset s, Node *n)
{
  if (!n || s.IsNull())  throw Exception();

  // can't bridge subgames
  if (s.rep->m_members.Length() > 0 && n->gameroot != s.rep->m_members[1]->gameroot)
    return 0;
  
  if (!n->infoset)   return 0; 
  if (n->infoset == s.rep)   return s;
  if (s.rep->m_actions.Length() != n->children.Length())  return n->infoset;

  m_revision++;
  m_dirty = true;

  gbt_efg_infoset_rep *t = n->infoset;

  t->m_members.Remove(t->m_members.Find(n));
  s.rep->m_members.Append(n);

  n->infoset = s.rep;

  DeleteLexicon();
  SortInfosets();
  return s;
}

gbtEfgInfoset efgGame::LeaveInfoset(Node *n)
{
  if (!n)  throw Exception();

  if (!n->infoset)   return 0;

  gbt_efg_infoset_rep *s = n->infoset;
  if (s->m_members.Length() == 1)   return s;

  m_revision++;
  m_dirty = true;

  gbt_efg_player_rep *p = s->m_player;
  s->m_members.Remove(s->m_members.Find(n));
  n->infoset = CreateInfoset(p->m_infosets.Length() + 1, p,
			     n->children.Length());
  n->infoset->m_label = s->m_label;
  n->infoset->m_members.Append(n);
  for (int i = 1; i <= s->m_actions.Length(); i++) {
    n->infoset->m_actions[i]->m_label = s->m_actions[i]->m_label;
  }

  DeleteLexicon();
  SortInfosets();
  return n->infoset;
}

gbtEfgInfoset efgGame::SplitInfoset(Node *n)
{
  if (!n)  throw Exception();

  if (!n->infoset)   return 0;

  gbt_efg_infoset_rep *s = n->infoset;
  if (s->m_members.Length() == 1)   return s;

  m_revision++;
  m_dirty = true;

  gbt_efg_player_rep *p = s->m_player;
  gbt_efg_infoset_rep *ns = CreateInfoset(p->m_infosets.Length() + 1, p,
					  n->children.Length());
  ns->m_label = s->m_label;
  for (int i = s->m_members.Length(); i > s->m_members.Find(n); i--)   {
    Node *nn = s->m_members.Remove(i);
    ns->m_members.Append(nn);
    nn->infoset = ns;
  }
  for (int i = 1; i <= s->m_actions.Length(); i++) {
    ns->m_actions[i]->m_label = s->m_actions[i]->m_label;
    if (p == chance) {
      ns->m_chanceProbs[i] = s->m_chanceProbs[i];
    }
  }
  DeleteLexicon();
  SortInfosets();
  return n->infoset;
}

gbtEfgInfoset efgGame::MergeInfoset(gbtEfgInfoset to, gbtEfgInfoset from)
{
  if (to.IsNull() || from.IsNull())  throw Exception();

  if (to == from ||
      to.rep->m_actions.Length() != from.rep->m_actions.Length())  {
    return from;
  }

  if (to.rep->m_members[1]->gameroot != from.rep->m_members[1]->gameroot) 
    return from;

  m_revision++;
  m_dirty = true;

  to.rep->m_members += from.rep->m_members;
  for (int i = 1; i <= from.rep->m_members.Length(); i++)
    from.rep->m_members[i]->infoset = to.rep;

  from.rep->m_members.Flush();

  DeleteLexicon();
  SortInfosets();
  return to;
}

bool efgGame::DeleteEmptyInfoset(gbtEfgInfoset s)
{
  if (s.IsNull())  throw Exception();

  if (s.NumMembers() > 0)   return false;

  m_revision++;
  m_dirty = true;
  s.rep->m_player->m_infosets.Remove(s.rep->m_player->m_infosets.Find(s.rep));
  delete s.rep;

  return true;
}

void efgGame::DeleteEmptyInfosets(void)
{
  for (int pl = 1; pl <= NumPlayers(); pl++) {
    for (int iset = 1; iset <= NumInfosets()[pl]; iset++) {
      if (DeleteEmptyInfoset(players[pl]->m_infosets[iset])) {
        iset--;
      }
    }
  }
} 

gbtEfgInfoset efgGame::SwitchPlayer(gbtEfgInfoset s, gbtEfgPlayer p)
{
  if (s.IsNull() || p.IsNull()) {
    throw Exception();
  }
  if (s.GetPlayer().IsChance() || p.IsChance()) {
    throw Exception();
  }
  
  if (s.rep->m_player == p.rep)   return s;

  m_revision++;
  m_dirty = true;
  s.rep->m_player->m_infosets.Remove(s.rep->m_player->m_infosets.Find(s.rep));
  s.rep->m_player = p.rep;
  p.rep->m_infosets.Append(s.rep);

  DeleteLexicon();
  SortInfosets();
  return s;
}

void efgGame::CopySubtree(Node *src, Node *dest, Node *stop)
{
  if (src == stop) {
    dest->outcome = src->outcome;
    return;
  }

  if (src->children.Length())  {
    AppendNode(dest, src->infoset);
    for (int i = 1; i <= src->children.Length(); i++)
      CopySubtree(src->children[i], dest->children[i], stop);
  }

  dest->name = src->name;
  dest->outcome = src->outcome;
}

//
// MarkSubtree: sets the Node::mark flag on all children of p_node
//
void efgGame::MarkSubtree(Node *p_node)
{
  p_node->mark = true;
  for (int i = 1; i <= p_node->children.Length(); i++) {
    MarkSubtree(p_node->children[i]);
  }
}

//
// UnmarkSubtree: clears the Node::mark flag on all children of p_node
//
void efgGame::UnmarkSubtree(Node *p_node)
{
  p_node->mark = false;
  for (int i = 1; i <= p_node->children.Length(); i++) {
    UnmarkSubtree(p_node->children[i]);
  }
}

void efgGame::Reveal(gbtEfgInfoset where, gbtEfgPlayer who)
{
  if (where.rep->m_actions.Length() <= 1)  {
    // only one action; nothing to reveal!
    return;
  }

  UnmarkSubtree(root);  // start with a clean tree
  
  m_revision++;
  m_dirty = true;

  for (int i = 1; i <= where.rep->m_actions.Length(); i++) {
    for (int j = 1; j <= where.rep->m_members.Length(); j++) { 
      MarkSubtree(where.rep->m_members[j]->children[i]);
    }

    for (int k = 1; k <= who.rep->m_infosets.Length(); k++) {
      // iterate over each member of information set 'k'
      // make copy of members to iterate correctly 
      // (since the information set may be changed in the process)
      gArray<Node *> members = who.rep->m_infosets[k]->m_members;
      gbtEfgInfoset newiset = 0;
      
      for (int m = 1; m <= members.Length(); m++) {
	Node *n = members[m];
	if (n->mark) {
	  // If node is marked, is descendant of action 'i'
	  n->mark = false;   // unmark so tree is clean at end
	  if (newiset.IsNull()) {
	    newiset = LeaveInfoset(n);
	  }
	  else {
	    JoinInfoset(newiset, n);
	  }
	} 
      }
    }
  }

  Reindex();
}

Node *efgGame::CopyTree(Node *src, Node *dest)
{
  if (!src || !dest)  throw Exception();
  if (src == dest || dest->children.Length())   return src;
  if (src->gameroot != dest->gameroot)  return src;

  if (src->children.Length())  {
    m_revision++;
    m_dirty = true;

    AppendNode(dest, src->infoset);
    for (int i = 1; i <= src->children.Length(); i++)
      CopySubtree(src->children[i], dest->children[i], dest);

    DeleteLexicon();
    SortInfosets();
  }

  return dest;
}

Node *efgGame::MoveTree(Node *src, Node *dest)
{
  if (!src || !dest)  throw Exception();
  if (src == dest || dest->children.Length() || IsPredecessor(src, dest))
    return src;
  if (src->gameroot != dest->gameroot)  return src;

  m_revision++;
  m_dirty = true;

  if (src->parent == dest->parent) {
    int srcChild = src->parent->children.Find(src);
    int destChild = src->parent->children.Find(dest);
    src->parent->children[srcChild] = dest;
    src->parent->children[destChild] = src;
  }
  else {
    Node *parent = src->parent; 
    parent->children[parent->children.Find(src)] = dest;
    dest->parent->children[dest->parent->children.Find(dest)] = src;
    src->parent = dest->parent;
    dest->parent = parent;
  }

  dest->name = "";
  dest->outcome = 0;
  
  DeleteLexicon();
  SortInfosets();
  return dest;
}

Node *efgGame::DeleteTree(Node *n)
{
  if (!n)  throw Exception();

  m_revision++;
  m_dirty = true;

  while (n->NumChildren() > 0)   {
    DeleteTree(n->children[1]);
    delete n->children.Remove(1);
  }
  
  if (n->infoset)  {
    n->infoset->m_members.Remove(n->infoset->m_members.Find(n));
    n->infoset = 0;
  }
  n->outcome = 0;
  n->name = "";

  DeleteLexicon();
  SortInfosets();
  return n;
}

gbtEfgAction efgGame::InsertAction(gbtEfgInfoset s)
{
  if (s.IsNull())  throw Exception();

  m_revision++;
  m_dirty = true;
  gbtEfgAction action = s.InsertAction(s.NumActions() + 1);
  for (int i = 1; i <= s.NumMembers(); i++) {
    s.rep->m_members[i]->children.Append(new Node(this, s.rep->m_members[i]));
  }
  DeleteLexicon();
  SortInfosets();
  return action;
}

gbtEfgAction efgGame::InsertAction(gbtEfgInfoset s, const gbtEfgAction &a)
{
  if (a.IsNull() || s.IsNull())  throw Exception();

  m_revision++;
  m_dirty = true;

  int where;
  for (where = 1; (where <= s.rep->m_actions.Length() &&
		   s.rep->m_actions[where] != a.rep);
       where++);
  if (where > s.rep->m_actions.Length())   return 0;
  gbtEfgAction action = s.InsertAction(where);
  for (int i = 1; i <= s.NumActions(); i++)
    s.rep->m_members[i]->children.Insert(new Node(this, s.rep->m_members[i]), where);

  DeleteLexicon();
  SortInfosets();
  return action;
}

gbtEfgInfoset efgGame::DeleteAction(gbtEfgInfoset s, const gbtEfgAction &a)
{
  if (a.IsNull() || s.IsNull())  throw Exception();

  m_revision++;
  m_dirty = true;
  int where;
  for (where = 1; (where <= s.rep->m_actions.Length() &&
		   s.rep->m_actions[where] != a.rep);
       where++);
  if (where > s.rep->m_actions.Length() || s.rep->m_actions.Length() == 1)
    return s;
  s.RemoveAction(where);
  for (int i = 1; i <= s.rep->m_members.Length(); i++)   {
    DeleteTree(s.rep->m_members[i]->children[where]);
    delete s.rep->m_members[i]->children.Remove(where);
  }
  DeleteLexicon();
  SortInfosets();
  return s;
}

void efgGame::SetChanceProb(gbtEfgInfoset infoset,
			    int act, const gNumber &value)
{
  if (infoset.IsChanceInfoset()) {
    m_revision++;
    m_dirty = true;
    infoset.SetChanceProb(act, value);
  }
}

gNumber efgGame::GetChanceProb(gbtEfgInfoset infoset, int act) const
{
  if (infoset.IsChanceInfoset())
    return infoset.GetChanceProb(act);
  else
    return (gNumber) 0;
}

gNumber efgGame::GetChanceProb(const gbtEfgAction &a) const
{
  return GetChanceProb(a.GetInfoset(), a.GetId());
}

//---------------------------------------------------------------------
//                     Subgame-related functions
//---------------------------------------------------------------------

void efgGame::MarkTree(Node *n, Node *base)
{
  n->ptr = base;
  for (int i = 1; i <= n->NumChildren(); i++)
    MarkTree(n->GetChild(i), base);
}

bool efgGame::CheckTree(Node *n, Node *base)
{
  int i;

  if (n->NumChildren() == 0)   return true;

  for (i = 1; i <= n->NumChildren(); i++)
    if (!CheckTree(n->GetChild(i), base))  return false;

  if (n->GetPlayer().IsChance())   return true;

  for (i = 1; i <= n->GetInfoset().NumMembers(); i++)
    if (n->GetInfoset().GetMember(i)->ptr != base)
      return false;

  return true;
}

bool efgGame::IsLegalSubgame(Node *n)
{
  if (n->NumChildren() == 0)  
    return false;

  MarkTree(n, n);
  return CheckTree(n, n);
}

bool efgGame::MarkSubgame(Node *n)
{
  if(n->gameroot == n) return true;

  if (n->gameroot != n && IsLegalSubgame(n))  {
    n->gameroot = 0;
    MarkSubgame(n, n);
    return true;
  }

  return false;
}

void efgGame::UnmarkSubgame(Node *n)
{
  if (n->gameroot == n && n->parent)  {
    n->gameroot = 0;
    MarkSubgame(n, n->parent->gameroot);
  }
}
  

void efgGame::MarkSubgame(Node *n, Node *base)
{
  if (n->gameroot == n)  return;
  n->gameroot = base;
  for (int i = 1; i <= n->NumChildren(); i++)
    MarkSubgame(n->GetChild(i), base);
}

void efgGame::MarkSubgames(void)
{
  gList<Node *> subgames;
  LegalSubgameRoots(*this, subgames);

  for (int i = 1; i <= subgames.Length(); i++)  {
    subgames[i]->gameroot = 0;
    MarkSubgame(subgames[i], subgames[i]);
  }
}

void efgGame::UnmarkSubgames(Node *n)
{
  if (n->NumChildren() == 0)   return;

  for (int i = 1; i <= n->NumChildren(); i++)
    UnmarkSubgames(n->GetChild(i));
  
  if (n->gameroot == n && n->parent)  {
    n->gameroot = 0;
    MarkSubgame(n, n->parent->gameroot);
  }
}


int efgGame::ProfileLength(void) const
{
  int sum = 0;

  for (int i = 1; i <= players.Length(); i++) {
    for (int j = 1; j <= players[i]->m_infosets.Length(); j++) {
      sum += players[i]->m_infosets[j]->m_actions.Length();
    }
  }

  return sum;
}

gArray<int> efgGame::NumInfosets(void) const
{
  gArray<int> foo(players.Length());
  
  for (int i = 1; i <= foo.Length(); i++) {
    foo[i] = players[i]->m_infosets.Length();
  }

  return foo;
}

int efgGame::NumPlayerInfosets(void) const
{
  int answer(0);
  for (int pl = 1; pl <= NumPlayers(); pl++) {
    answer +=  players[pl]->m_infosets.Length();
  }
  return answer;
}

int efgGame::NumChanceInfosets(void) const
{
  return chance->m_infosets.Length();
}

int efgGame::TotalNumInfosets(void) const
{
  return NumPlayerInfosets() + NumChanceInfosets();
}

gPVector<int> efgGame::NumActions(void) const
{
  gArray<int> foo(players.Length());
  for (int i = 1; i <= players.Length(); i++) {
    foo[i] = players[i]->m_infosets.Length();
  }

  gPVector<int> bar(foo);
  for (int i = 1; i <= players.Length(); i++) {
    for (int j = 1; j <= players[i]->m_infosets.Length(); j++) {
      bar(i, j) = players[i]->m_infosets[j]->m_actions.Length();
    }
  }

  return bar;
}  

int efgGame::NumPlayerActions(void) const
{
  int answer = 0;

  gPVector<int> nums_actions = NumActions();
  for (int i = 1; i <= NumPlayers(); i++)
    answer += nums_actions[i];
  return answer;
}

gPVector<int> efgGame::NumMembers(void) const
{
  gArray<int> foo(players.Length());

  for (int i = 1; i <= players.Length(); i++) {
    foo[i] = players[i]->m_infosets.Length();
  }

  gPVector<int> bar(foo);
  for (int i = 1; i <= players.Length(); i++) {
    for (int j = 1; j <= players[i]->m_infosets.Length(); j++) {
      bar(i, j) = players[i]->m_infosets[j]->m_members.Length();
    }
  }

  return bar;
}

//------------------------------------------------------------------------
//                       Efg: Payoff computation
//------------------------------------------------------------------------

void efgGame::Payoff(Node *n, gNumber prob, const gPVector<int> &profile,
		 gVector<gNumber> &payoff) const
{
  if (n->outcome)  {
    for (int i = 1; i <= players.Length(); i++)
      payoff[i] += prob * n->outcome->m_payoffs[i];
  }

  if (n->infoset && n->infoset->m_player->m_id == 0) {
    for (int i = 1; i <= n->children.Length(); i++) {
      Payoff(n->children[i],
	     prob * GetChanceProb(n->infoset, i),
	     profile, payoff);
    }
  }
  else if (n->infoset) {
    Payoff(n->children[profile(n->infoset->m_player->m_id, n->infoset->m_id)],
	   prob, profile, payoff);
  }
}

void efgGame::InfosetProbs(Node *n, gNumber prob, const gPVector<int> &profile,
			  gPVector<gNumber> &probs) const
{
  if (n->infoset && n->infoset->m_player->m_id == 0) {
    for (int i = 1; i <= n->children.Length(); i++) {
      InfosetProbs(n->children[i],
		   prob * GetChanceProb(n->infoset, i),
		   profile, probs);
    }
  }
  else if (n->infoset)  {
    probs(n->infoset->m_player->m_id, n->infoset->m_id) += prob;
    InfosetProbs(n->children[profile(n->infoset->m_player->m_id, 
				     n->infoset->m_id)],
		 prob, profile, probs);
  }
}

void efgGame::Payoff(const gPVector<int> &profile, gVector<gNumber> &payoff) const
{
  ((gVector<gNumber> &) payoff).operator=((gNumber) 0);
  Payoff(root, 1, profile, payoff);
}

void efgGame::InfosetProbs(const gPVector<int> &profile,
			  gPVector<gNumber> &probs) const
{
  ((gVector<gNumber> &) probs).operator=((gNumber) 0);
  InfosetProbs(root, 1, profile, probs);
}

void efgGame::Payoff(Node *n, gNumber prob, const gArray<gArray<int> *> &profile,
		    gArray<gNumber> &payoff) const
{
  if (n->outcome)   {
    for (int i = 1; i <= players.Length(); i++)
      payoff[i] += prob * n->outcome->m_payoffs[i];
  }
  
  if (n->infoset && n->infoset->m_player->m_id == 0) {
    for (int i = 1; i <= n->children.Length(); i++) {
      Payoff(n->children[i],
	     prob * GetChanceProb(n->infoset, i),
	     profile, payoff);
    }
  }
  else if (n->infoset) {
    Payoff(n->children[(*profile[n->infoset->m_player->m_id])[n->infoset->m_id]],
	   prob, profile, payoff);
  }
}

void efgGame::Payoff(const gArray<gArray<int> *> &profile,
		 gArray<gNumber> &payoff) const
{
  for (int i = 1; i <= payoff.Length(); i++)
    payoff[i] = 0;
  Payoff(root, 1, profile, payoff);
}

Nfg *efgGame::AssociatedNfg(void) const
{
  if (lexicon) {
    return lexicon->N;
  }
  else {
    return 0;
  }
}

Nfg *efgGame::AssociatedAfg(void) const
{
  return afg;
}

