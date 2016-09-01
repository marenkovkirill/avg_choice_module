/* INCLUDE CONFIG */
/* General */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sqlite3.h>

#include <string>
#include <set>
#include <map>
#include <vector>

#ifdef _WIN32
//#include "stringC11.h"
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

  if (not ((count_functions == 1) && count_robots)) {
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
    "select\n"     
    "  s.iid,\n"
    "  ru.uid,\n"
    "avg(fc.end - fc.start) as avg_time\n"
    "from\n"
    "  function_calls as fc,\n"
    "  robot_uids as ru,\n"
    "  sources as s,\n"
    "  functions as f,\n"
    "  contexts as c\n"
    "where\n"
    "  fc.function_id = f.id\n"
    "  and f.context_id = c.id\n"
    "  and fc.robot_id = ru.id\n"
    "  and ru.source_id = s.id\n"

    "  /* calls restrict */\n"
    "  and fc.success = 1\n"
    "  and fc.end is not NULL\n";


  string functions_restrict = "and ";
  const ChoiceFunctionData *func_data = function_data[0];

  functions_restrict += "f.name = '" + (string)(func_data->name) + 
                        "'\nand " +
                        "c.hash = '" + (string)(func_data->context_hash) + 
                        "'\n";

  map<string, vector<string>> modulesRobots;
  bool is_empty_name = false;
  for (int i = 0; i < count_robots; ++i) {
    const char* check_uid = robots_data[i]->robot_uid;
    
    if (check_uid != NULL) {
      string uid(check_uid);
      if (uid.empty()){
        is_empty_name = true;
        break;
      }
      const char* check_iid = robots_data[i]->module_data->iid;
      string tmpIid(check_iid);

      colorPrintf(ConsoleColor(ConsoleColor::yellow), "%d iid: %s\n",
        i, tmpIid.c_str());

      if (modulesRobots.count(tmpIid)) {
        modulesRobots[tmpIid].push_back(uid);
      } else {
        vector<string> newVector;
        newVector.push_back(uid);
        modulesRobots[tmpIid] = newVector;
      }
    } else {
      is_empty_name = true;
      break;
    }
  }

  
  string robots_restrict("");

  if (!is_empty_name) {              
    for (auto robots = modulesRobots.begin(); robots != modulesRobots.end(); ++robots) {

      string robots_uid("");
      auto robots_names = robots->second;
      for (int i = 0; i < robots_names.size(); ++i) {
        if (!robots_uid.empty()) {
          robots_uid += ",";
        }
        robots_uid += "'" + robots_names[i] + "'";
      }

      robots_uid = "(" + robots_uid + ")";

      if (!robots_restrict.empty()) {
        robots_restrict += "or ";
      }

      robots_restrict += string("(\n") + 
                         "s.iid = '" + robots->first + "'\nand " +
                         "s.type = 2\nand " +
                         "ru.uid in" + robots_uid + "\n)\n";
    }

    robots_restrict = "and (\n" + robots_restrict + ")\n";
  }

  sql_query += functions_restrict + robots_restrict + 
               "group by s.iid,ru.uid order by avg_time ASC limit 1";

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

  const ChoiceRobotData *p_res = NULL;
  if (num_col > 0) {
    for (uint i = 0; i < count_robots && p_res == NULL; i++) {
      const ChoiceRobotData *data = robots_data[i];
  
      if ((string)data->module_data->iid == (string)sql_result[num_col] &&
          (string)data->robot_uid == (string)sql_result[num_col + 1]) {
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
