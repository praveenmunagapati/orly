/* <stig/indy/context.cc>

   Implements <stig/indy/context.h>.

   Copyright 2010-2014 Stig LLC

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <stig/indy/context.h>

#include <stig/var/sabot_to_var.h>

using namespace std;
using namespace Base;
using namespace Stig;
using namespace Stig::Indy;

#if 0
__thread TContext::TKeyCursorCollection::TImpl *TContext::KeyCursorCollection;

static TContext::TKeyCursorCollection::TImpl *CheckConstructKeyCursor(TContext *context) {
  if (!TContext::KeyCursorCollection) {
    TContext::KeyCursorCollection = new TContext::TKeyCursorCollection::TImpl(context);
  }
  return TContext::KeyCursorCollection;
}

static void CheckDestroyKeyCursor() {
  if (TContext::KeyCursorCollection && TContext::KeyCursorCollection->IsEmpty()) {
    delete TContext::KeyCursorCollection;
    TContext::KeyCursorCollection = nullptr;
  }
}
#endif

TContext::TContext(const Indy::L0::TManager::TPtr<TRepo> &private_repo, Atom::TCore::TExtensibleArena *arena)
    : TContextBase(arena), WalkerCount(0UL) {
  Indy::L0::TManager::TPtr<L0::TManager::TRepo> cur_repo = private_repo;
  RepoTree.push_back(make_pair(private_repo, unique_ptr<TRepo::TView>(new TRepo::TView(private_repo))));
  for (;cur_repo->GetParentRepo(); cur_repo = *cur_repo->GetParentRepo()) {
    Indy::L0::TManager::TPtr<Indy::TRepo> parent = *cur_repo->GetParentRepo();
    RepoTree.push_back(make_pair(parent, unique_ptr<TRepo::TView>(new TRepo::TView(parent))));
  }
}

TContext::~TContext() {}

Indy::TKey TContext::operator[](const Indy::TIndexKey &index_key) {
  /* check to see if any of our current key cursors are on this key.
     We're doing this as a quick fix to the fact that we've lost which cursor (if any) this key
     originated from in the code gen. (loss of information). */
  /* we need to re-implement this caching strategy without a thread_local since we're using fibers now... */
  #if 0
  if (KeyCursorCollection) {
    const auto &key = index_key.GetKey();
    for (TKeyCursorCollection::TCursor csr(KeyCursorCollection); csr; ++csr) {
      const Indy::TPresentWalker::TItem &cur_item = csr->GetVal();
      if (Indy::TKey::EqEq(cur_item.Key, cur_item.KeyArena, key.GetCore(), key.GetArena())) {
        /* we found a cursor that is on the key we're looking for. let's just return the result. */
        return Indy::TKey(cur_item.Op, cur_item.OpArena);
      }
    }
  }
  #endif
  /* no dice. we're going to have to make a new walker. This suggests someone is reading an explicit
     key (as opposed to one generated by the "keys" expression), or we've manipulated / collected keys
     to the point where the cursor has advanced past what we're trying to read. */
  ++WalkerCount;
  TPresentWalker walker(this, RepoTree, index_key);
  if (walker) {
    const Indy::TPresentWalker::TItem &item = *walker;
    return Indy::TKey(Atom::TCore(GetArena(), alloca(Sabot::State::GetMaxStateSize()), item.OpArena, item.Op), GetArena());
  }
  /* We return an empty var here because in the case of an optional type being returned, the result is "empty" not a throw.
     A wrapper promotes empty -> throw if we need to end up as not an optional. */
  return Indy::TKey(Atom::TCore(), nullptr);
}

bool TContext::Exists(const Indy::TIndexKey &key) {
  ++WalkerCount;
  TPresentWalker walker(this, RepoTree, key);
  return static_cast<bool>(walker);
}

TContext::TPresentWalker::TPresentWalker(TContext *ctx, const TRepoTree &repo_tree, const TIndexKey &key)
    : MinHeap(repo_tree.size()),
      Valid(false) {
  assert(Fiber::TFrame::LocalFramePool);
  size_t pos = 0;
  ctx->PresentWalkConsTimer.Start();
  //printf("TPresentWalker()\n");
  for (const auto &iter : repo_tree) {
    //printf("TPresentWalker() push back repo walker\n");
    WalkerVec.emplace_back(iter.first->NewPresentWalker(iter.second, key));
    //Indy::TPresentWalker &walker = *WalkerVec.back();
  }
  for (auto &walker_ptr : WalkerVec) {
    Indy::TPresentWalker &walker = *walker_ptr;
    if (walker) {
      MinHeap.Insert(*walker, pos);
    }
    ++pos;
  }
  Valid = static_cast<bool>(MinHeap);
  Refresh();
  ctx->PresentWalkConsTimer.Stop();
}

TContext::TPresentWalker::TPresentWalker(TContext *ctx, const TRepoTree &repo_tree, const TIndexKey &from, const TIndexKey &to)
    : MinHeap(repo_tree.size()),
      Valid(false) {
  ctx->PresentWalkConsTimer.Start();
  size_t pos = 0;
  for (const auto &iter : repo_tree) {
    WalkerVec.emplace_back(iter.first->NewPresentWalker(iter.second, from, to, false));
    Indy::TPresentWalker &walker = *WalkerVec.back();
    if (walker) {
      MinHeap.Insert(*walker, pos);
    }
    ++pos;
  }
  Valid = static_cast<bool>(MinHeap);
  Refresh();
  ctx->PresentWalkConsTimer.Stop();
}

TContext::TPresentWalker::~TPresentWalker() {}

void TContext::TPresentWalker::Refresh() {
  assert(this);
  bool done = false;
  size_t pos;
  while (Valid) {
    const Indy::TPresentWalker::TItem &cur_item = MinHeap.Pop(pos);
    Indy::TPresentWalker &walker = *WalkerVec[pos];
    assert((*walker).KeyArena == cur_item.KeyArena);
    assert((*walker).OpArena == cur_item.OpArena);
    assert((*walker).SequenceNumber == cur_item.SequenceNumber);
    if (Item.KeyArena == nullptr || (Indy::TKey::TupleNeEq(Item.Key, Item.KeyArena, cur_item.Key, cur_item.KeyArena))) {
      Item = cur_item;
      if (!Item.Op.IsTombstone()) {
        done = true;
      }
    }
    ++walker;
    if (walker) {
      MinHeap.Insert(*walker, pos);
    }
    if (!done) {
      Valid = static_cast<bool>(MinHeap);
    } else {
      break;
    }
  }
}

TContext::TKeyCursor::TKeyCursor(TContext *context, const Indy::TIndexKey &pattern)
    : Stig::TKeyCursor(context->Arena),
      Key(pattern),
      Valid(true),
      Cached(false),
      Csr(context, context->RepoTree, Key),
      ContextMembership(this) {
  /* re-think thread_local caching strategy for fibers... */
  #if 0
  MyCollection = CheckConstructKeyCursor(context);
  #endif
  ++(context->WalkerCount);
}

TContext::TKeyCursor::TKeyCursor(TContext *context, const Indy::TIndexKey &from, const Indy::TIndexKey &to)
    : Stig::TKeyCursor(context->Arena),
      Key(from),
      To(to),
      Valid(true),
      Cached(false),
      Csr(context, context->RepoTree, Key, To),
      ContextMembership(this) {
  /* re-think thread_local caching strategy for fibers... */
  #if 0
  MyCollection = CheckConstructKeyCursor(context);
  #endif
  ++(context->WalkerCount);
}

TContext::TKeyCursor::~TKeyCursor() {
  assert(this);
  ContextMembership.Remove();
  /* re-think thread_local caching strategy for fibers... */
  #if 0
  CheckDestroyKeyCursor();
  #endif
}

TContext::TKeyCursor::operator bool() const {
  assert(this);
  Refresh();
  return Valid;
}

const Indy::TKey &TContext::TKeyCursor::operator*() const {
  assert(this);
  Refresh();
  assert(Valid);
  assert(Cached);
  return Item;
}

const Indy::TKey *TContext::TKeyCursor::operator->() const {
  assert(this);
  Refresh();
  assert(Valid);
  assert(Cached);
  return &Item;
}

TContext::TKeyCursor &TContext::TKeyCursor::operator++() {
  assert(this);
  assert(Valid);
  Cached = false;
  ++Csr;
  ContextMembership.Remove();
  Refresh();
  return *this;
}

const TContext::TPresentWalker::TItem &TContext::TKeyCursor::GetVal() const {
  assert(this);
  assert(Valid);
  assert(Csr);
  return *Csr;
}

void TContext::TKeyCursor::Refresh() const {
  if (Valid && !Cached) {
    Cached = true;
    if (Csr) {
      Item = Indy::TKey((*Csr).Key, (*Csr).KeyArena);
      /* insert ourselves into the thread local key cursor collection */
      /* re-think thread_local caching strategy for fibers... */
      #if 0
      assert(MyCollection);
      ContextMembership.Insert(MyCollection);
      #endif
    } else {
      Valid = false;
    }
  }
}