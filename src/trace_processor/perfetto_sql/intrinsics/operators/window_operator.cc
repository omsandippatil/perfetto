/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/perfetto_sql/intrinsics/operators/window_operator.h"

#include <sqlite3.h>
#include <cstdint>
#include <memory>

#include "perfetto/base/logging.h"
#include "src/trace_processor/sqlite/bindings/sqlite_result.h"
#include "src/trace_processor/sqlite/sqlite_utils.h"

namespace perfetto::trace_processor {

namespace {
constexpr char kSchema[] = R"(
    CREATE TABLE x(
      rowid BIGINT HIDDEN,
      quantum BIGINT HIDDEN,
      window_start BIGINT HIDDEN,
      window_dur BIGINT HIDDEN,
      ts BIGINT,
      dur BIGINT,
      quantum_ts BIGINT,
      PRIMARY KEY(rowid)
    ) WITHOUT ROWID
  )";
}

int WindowOperatorModule::Create(sqlite3* db,
                                 void* raw_ctx,
                                 int argc,
                                 const char* const* argv,
                                 sqlite3_vtab** vtab,
                                 char**) {
  PERFETTO_CHECK(argc == 3);
  if (int ret = sqlite3_declare_vtab(db, kSchema); ret != SQLITE_OK) {
    return ret;
  }
  auto* ctx = GetContext(raw_ctx);
  auto it_and_inserted = ctx->state_by_name.Insert(argv[2], nullptr);
  PERFETTO_CHECK(
      it_and_inserted.second ||
      (it_and_inserted.first && it_and_inserted.first->get()->disconnected));
  *it_and_inserted.first = std::make_unique<State>();

  std::unique_ptr<Vtab> res = std::make_unique<Vtab>();
  res->context = ctx;
  res->name = argv[2];
  res->state = it_and_inserted.first->get();
  *vtab = res.release();
  return SQLITE_OK;
}

int WindowOperatorModule::Destroy(sqlite3_vtab* vtab) {
  auto* tab = GetVtab(vtab);
  PERFETTO_CHECK(tab->context->state_by_name.Erase(tab->name));
  delete tab;
  return SQLITE_OK;
}

int WindowOperatorModule::Connect(sqlite3* db,
                                  void* raw_ctx,
                                  int argc,
                                  const char* const* argv,
                                  sqlite3_vtab** vtab,
                                  char**) {
  PERFETTO_CHECK(argc == 3);
  if (int ret = sqlite3_declare_vtab(db, kSchema); ret != SQLITE_OK) {
    return ret;
  }
  auto* ctx = GetContext(raw_ctx);
  auto* ptr = ctx->state_by_name.Find(argv[2]);
  PERFETTO_CHECK(ptr);
  ptr->get()->disconnected = false;

  std::unique_ptr<Vtab> res = std::make_unique<Vtab>();
  res->context = ctx;
  res->name = argv[2];
  res->state = ptr->get();
  *vtab = res.release();
  return SQLITE_OK;
}

int WindowOperatorModule::Disconnect(sqlite3_vtab* vtab) {
  auto* tab = GetVtab(vtab);
  auto* ptr = tab->context->state_by_name.Find(tab->name);
  PERFETTO_CHECK(ptr);
  ptr->get()->disconnected = true;
  delete tab;
  return SQLITE_OK;
}

int WindowOperatorModule::BestIndex(sqlite3_vtab*, sqlite3_index_info* info) {
  info->orderByConsumed = info->nOrderBy == 1 &&
                          info->aOrderBy[0].iColumn == Column::kTs &&
                          !info->aOrderBy[0].desc;

  // Set return first if there is a equals constraint on the row id asking to
  // return the first row.
  bool is_row_id_constraint = info->nConstraint == 1 &&
                              info->aConstraint[0].iColumn == Column::kRowId &&
                              info->aConstraint[0].usable &&
                              sqlite::utils::IsOpEq(info->aConstraint[0].op);
  if (is_row_id_constraint) {
    info->idxNum = 1;
    info->aConstraintUsage[0].argvIndex = 1;
  } else {
    info->idxNum = 0;
  }
  return SQLITE_OK;
}

int WindowOperatorModule::Open(sqlite3_vtab*, sqlite3_vtab_cursor** cursor) {
  std::unique_ptr<Cursor> c = std::make_unique<Cursor>();
  *cursor = c.release();
  return SQLITE_OK;
}

int WindowOperatorModule::Close(sqlite3_vtab_cursor* cursor) {
  delete GetCursor(cursor);
  return SQLITE_OK;
}

int WindowOperatorModule::Filter(sqlite3_vtab_cursor* cursor,
                                 int is_row_id_constraint,
                                 const char*,
                                 int argc,
                                 sqlite3_value** argv) {
  auto* t = GetVtab(cursor->pVtab);
  auto* c = GetCursor(cursor);

  c->window_start = t->state->window_start;
  c->window_end = t->state->window_start + t->state->window_dur;
  c->step_size =
      t->state->quantum == 0 ? t->state->window_dur : t->state->quantum;

  c->current_ts = c->window_start;

  if (is_row_id_constraint) {
    PERFETTO_CHECK(argc == 1);
    c->filter_type = sqlite3_value_int(argv[0]) == 0 ? FilterType::kReturnFirst
                                                     : FilterType::kReturnAll;
  } else {
    c->filter_type = FilterType::kReturnAll;
  }
  return SQLITE_OK;
}

int WindowOperatorModule::Next(sqlite3_vtab_cursor* cursor) {
  auto* c = GetCursor(cursor);
  switch (c->filter_type) {
    case FilterType::kReturnFirst:
      c->current_ts = c->window_end;
      break;
    case FilterType::kReturnAll:
      c->current_ts += c->step_size;
      c->quantum_ts++;
      break;
  }
  c->row_id++;
  return SQLITE_OK;
}

int WindowOperatorModule::Eof(sqlite3_vtab_cursor* cursor) {
  auto* c = GetCursor(cursor);
  return c->current_ts >= c->window_end;
}

int WindowOperatorModule::Column(sqlite3_vtab_cursor* cursor,
                                 sqlite3_context* ctx,
                                 int N) {
  auto* t = GetVtab(cursor->pVtab);
  auto* c = GetCursor(cursor);
  switch (N) {
    case Column::kQuantum: {
      sqlite::result::Long(ctx, static_cast<sqlite_int64>(t->state->quantum));
      break;
    }
    case Column::kWindowStart: {
      sqlite::result::Long(ctx,
                           static_cast<sqlite_int64>(t->state->window_start));
      break;
    }
    case Column::kWindowDur: {
      sqlite::result::Long(ctx, static_cast<int>(t->state->window_dur));
      break;
    }
    case Column::kTs: {
      sqlite::result::Long(ctx, static_cast<sqlite_int64>(c->current_ts));
      break;
    }
    case Column::kDuration: {
      sqlite::result::Long(ctx, static_cast<sqlite_int64>(c->step_size));
      break;
    }
    case Column::kQuantumTs: {
      sqlite::result::Long(ctx, static_cast<sqlite_int64>(c->quantum_ts));
      break;
    }
    case Column::kRowId: {
      sqlite::result::Long(ctx, static_cast<sqlite_int64>(c->row_id));
      break;
    }
    default: {
      PERFETTO_FATAL("Unknown column %d", N);
      break;
    }
  }
  return SQLITE_OK;
}

int WindowOperatorModule::Rowid(sqlite3_vtab_cursor*, sqlite_int64*) {
  return SQLITE_ERROR;
}

int WindowOperatorModule::Update(sqlite3_vtab* tab,
                                 int argc,
                                 sqlite3_value** argv,
                                 sqlite_int64*) {
  auto* t = GetVtab(tab);

  // We only support updates to ts and dur. Disallow deletes (argc == 1) and
  // inserts (argv[0] == null).
  if (argc < 2 || sqlite3_value_type(argv[0]) == SQLITE_NULL) {
    return sqlite::utils::SetError(
        tab, "Invalid number/value of arguments when updating window table");
  }

  int64_t new_quantum = sqlite3_value_int64(argv[3]);
  int64_t new_start = sqlite3_value_int64(argv[4]);
  int64_t new_dur = sqlite3_value_int64(argv[5]);
  if (new_dur == 0) {
    return sqlite::utils::SetError(
        tab, "Cannot set duration of window table to zero.");
  }

  t->state->quantum = new_quantum;
  t->state->window_start = new_start;
  t->state->window_dur = new_dur;

  return SQLITE_OK;
}

}  // namespace perfetto::trace_processor
