/* INCLUDE CONFIG */
/* General */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sqlite3.h>
#include <string>

#ifdef _WIN32
#include "stringC11.h"
#endif

#include "SimpleIni.h"

/* RCML */
#include "module.h"
#include "choice_module.h"
#include "avg_choice_module.h"

/* GLOBALS CONFIG */
#define IID "RCT.AVG_choise_module_v101"
typedef unsigned int uint;

AvgChoiceModule::AvgChoiceModule() {
  mi = new ModuleInfo;
  mi->uid = IID;
  mi->mode = ModuleInfo::Modes::PROD;
  mi->version = BUILD_NUMBER;
  mi->digest = NULL;
}

const struct ModuleInfo &AvgChoiceModule::getModuleInfo() { return *mi; }

void AvgChoiceModule::prepare(colorPrintfModule_t *colorPrintf_p,
                              colorPrintfModuleVA_t *colorPrintfVA_p) {
  this->colorPrintf_p = colorPrintfVA_p;
}

void *AvgChoiceModule::writePC(unsigned int *buffer_length) {
  (*buffer_length) = 0;
  return NULL;
}

int AvgChoiceModule::init() {
  std::string config_path = "config.ini";

  CSimpleIniA ini;
  ini.SetMultiKey(true);

  if (ini.LoadFile(config_path.c_str()) < 0) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't load '%s' file!",
                config_path.c_str());
    return 1;
  }

  db_path = ini.GetValue("statisctic", "db_path", NULL);
  if (db_path.empty()) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't read db_path value");
    return 1;
  }

  sqlite3 *db;
  int rc = sqlite3_open(db_path.c_str(), &db);
  if (rc) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't open database: %s\n",
                sqlite3_errmsg(db));
    sqlite3_close(db);
    return (1);
  }
  sqlite3_close(db);

  return 0;
}

void AvgChoiceModule::final() {}

int AvgChoiceModule::readPC(int pc_index, void *buffer,
                            unsigned int buffer_length) {
  return 0;
}

int AvgChoiceModule::startProgram(int run_index, int pc_index) { return 0; }

const ChoiceRobotData *AvgChoiceModule::makeChoice(int run_index,
    const ChoiceFunctionData **function_data, uint count_functions,
    const ChoiceRobotData **robots_data, uint count_robots) {
  if (not (count_functions && count_robots)) {
     return NULL;
  }

  sqlite3 *db;
  int rc = sqlite3_open(db_path.c_str(), &db);
  if (rc) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "Can't open database: %s\n",
                sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }

  string sql_query =
      "with funcs as (select f.id\n"
      "                 from functions f\n"
      "                 join contexts c\n"
      "                   on f.context_id = c.id\n"
      "                where (%FUNCS_CLAUSE%"
      "                       )\n"
      "		      ),\n"
      "     robots as (select ru.id\n"
      "                  from robot_uids ru\n"
      "                  join sources s\n"
      "                    on (ru.source_id = s.id)\n"
      "	          where (%ROBOTS_CLAUSE%"
      "                  )\n"
      "                ),\n"
      "     fc as (select fc.robot_id,\n"
      "                   count(distinct funcs.id) as f_cnt,\n"
      "                   avg(fc.end-fc.start) as avg_time\n"
      "              from function_calls fc\n"
      "              join robots on (fc.robot_id = robots.id)\n"
      "              join funcs on (funcs.id = fc.function_id)\n"
      "             group by fc.robot_id\n"
      "            )\n"
      "select rt.uid, 1 as pos, 0 as f_cnt, 0 as avg_time\n"
      "  from (%ROBOTS_TABLE%"
      "	       ) rt\n"
      "	where not exists (select 1 from robot_uids ru where ru.uid = rt.uid)\n"
      " union\n"
      " select uid, pos, f_cnt, avg_time\n"
      "   from (select ru.uid, 2 as pos, fc.f_cnt, fc.avg_time\n"
      "           from robot_uids ru\n"
      "           left join fc\n"
      "             on (fc.robot_id = ru.id)\n"
      "         ) b\n"
      " order by pos, f_cnt, avg_time\n"
      " limit 1\n";


  string functions_clause = "";
  for (uint i = 0; i < count_functions; i++) {
    if (i >= 1) {
      functions_clause += "or ";
    }
    functions_clause += "(f.name = '" + (string)(function_data[i]->name) +
                        "'\n"
                        " and f.position = " +
                        to_string(function_data[i]->position) +
                        "\n"
                        " and c.hash = '" +
                        (string)(function_data[i]->context_hash) + "')\n";
  }
  sql_query.replace(sql_query.find("%FUNCS_CLAUSE%"), 14, functions_clause);

  string robots_table = "";
uint uid_count = 0;
string robots_clause =
    "   %ROBOT_UIDS%\n"
    "or %SOURCE_HASHES%\n";
string uids = "";
string hashes = "";
for (uint i = 0; i < count_robots; i++) {
  if (robots_data[i]->robot_uid == 0 or robots_data[i]->robot_uid == NULL or
      (string)(robots_data[i]->robot_uid) == (string) "") {
    hashes += "'" + (string)(robots_data[i]->module_data->hash) + "',";
  } else {
    uid_count += 1;
    uids += "'" + (string)(robots_data[i]->robot_uid) + "',";
    if (uid_count > 1) {
      robots_table += "UNION\n";
    }
    robots_table +=
        "select '" + (string)(robots_data[i]->robot_uid) + "' as uid\n";
  }
}
if (uids == (string) "") {
  uids = "1=0";
} else {
  uids = "ru.uid in (" + uids.substr(0, uids.length() - 1) + ")";
}
if (hashes == (string) "") {
  hashes = "1=0";
} else {
  hashes = "s.hash in (" + hashes.substr(0, hashes.length() - 1) + ")";
}
robots_clause.replace(robots_clause.find("%ROBOT_UIDS%"), 12, uids);
robots_clause.replace(robots_clause.find("%SOURCE_HASHES%"), 15, hashes);
sql_query.replace(sql_query.find("%ROBOTS_CLAUSE%"), 15, robots_clause);
sql_query.replace(sql_query.find("%ROBOTS_TABLE%"), 14, robots_table);

#ifdef IS_DEBUG
  colorPrintf(ConsoleColor(ConsoleColor::yellow), "SQL statement:\n%s\n\n",
              sql_query.c_str());
#endif

  int num_row = 0;
  int num_col = 0;
  char *error_message = 0;
  char **sql_result = 0;
  if (sqlite3_get_table(db, sql_query.c_str(), &sql_result, &num_row, &num_col,
                        &error_message) != SQLITE_OK) {
    colorPrintf(ConsoleColor(ConsoleColor::red), "SQL error:\n%s\n\n",
                error_message);
    sqlite3_free(error_message);
    sqlite3_close(db);
    return NULL;
  }
#ifdef IS_DEBUG
  colorPrintf(ConsoleColor(ConsoleColor::yellow), "SQL result:\n%s\n\n",
              num_col > 0 ? sql_result[4] : "NULL");
#endif

  const ChoiceRobotData *p_res = NULL;
  if (num_col > 0) {
    for (uint i = 0; i < count_robots; i++) {
      if ((string)(robots_data[i]->robot_uid) == (string)sql_result[4]) {
        p_res = robots_data[i];
      }
    }
  }

#ifdef IS_DEBUG
  string result = "NULL";
  if (p_res != NULL) {
    result = (string)(p_res->robot_uid);
  }
  colorPrintf(ConsoleColor(ConsoleColor::yellow), "MakeChoice result:\n%s\n\n",
              result.c_str());
#endif
  sqlite3_free_table(sql_result);
  sqlite3_close(db);

  return p_res;
}

int AvgChoiceModule::endProgram(int run_index) { return 0; }

void AvgChoiceModule::destroy() {
  delete mi;
  delete this;
}

void AvgChoiceModule::colorPrintf(ConsoleColor colors, const char *mask, ...) {
  va_list args;
  va_start(args, mask);
  (*colorPrintf_p)(this, colors, mask, args);
  va_end(args);
}

PREFIX_FUNC_DLL unsigned short getChoiceModuleApiVersion() {
  return MODULE_API_VERSION;
}
PREFIX_FUNC_DLL ChoiceModule *getChoiceModuleObject() {
  return new AvgChoiceModule();
}
