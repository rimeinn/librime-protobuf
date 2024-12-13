#include <algorithm>
#include <iostream>
#include <string>
#include "src/rime_proto_api.h"
#include <rime_api.h>
#include <rime_proto.pb.h>

void print_status(const rime::proto::Status &status) {
  std::cout << "schema: " << status.schemaid()
            << " / " << status.schemaname() << std::endl;
  std::cout << "status:";
  if (status.isdisabled()) std::cout << " disabled";
  if (status.iscomposing()) std::cout << " composing";
  if (status.isasciimode()) std::cout << " ascii";
  if (status.isfullshape()) std::cout << " full_shape";
  if (status.issimplified()) std::cout << " simplified";
  std::cout << std::endl;
}

void print_composition(const rime::proto::Context::Composition &composition) {
  if (composition.preedit().empty()) return;
  std::string preedit = composition.preedit();
  size_t len = composition.length();
  size_t start = composition.selstart();
  size_t end = composition.selend();
  size_t cursor = composition.cursorpos();
  for (size_t i = 0; i <= len; ++i) {
    if (start < end) {
      if (i == start) {
        std::cout << '[';
      } else if (i == end) {
        std::cout << ']';
      }
    }
    if (i == cursor) std::cout << '|';
    if (i < len)
      std::cout << preedit[i];
  }
  std::cout << std::endl;
}

void print_menu(const rime::proto::Context::Menu &menu) {
  int num_candidates = menu.candidates_size();
  if (num_candidates == 0) return;
  std::cout << "page: " << (menu.pagenumber() + 1)
            << (menu.islastpage() ? '$' : ' ')
            << " (of size " << menu.pagesize() << ")" << std::endl;
  for (int i = 0; i < num_candidates; ++i) {
    bool highlighted = i == menu.highlightedcandidateindex();
    auto candidate = menu.candidates()[i];
    std::cout << (i + 1) << ". "
              << (highlighted ? '[' : ' ')
              << candidate.text()
              << (highlighted ? ']' : ' ');
    if (candidate.has_comment())
      std::cout << candidate.comment();
    std::cout << std::endl;
  }
}

void print_context(rime::proto::Context &context) {
  auto composition = context.composition();
  if (composition.length() > 0) {
    print_composition(composition);
    auto menu = context.menu();
    print_menu(menu);
  } else {
    std::cout << "(not composing)" << std::endl;
  }
}

void print(RimeSessionId session_id) {
  RimeApi* rime = rime_get_api();
  RimeProtoApi* proto = (RimeProtoApi*) rime->find_module("proto")->get_api();

  rime::proto::Commit commit_;
  proto->commit_proto(session_id, &commit_);
  if (commit_.has_text()) {
    std::cout << "commit: " << commit_.text() << std::endl;
  }

  rime::proto::Status status_;
  proto->status_proto(session_id, &status_);
  print_status(status_);

  rime::proto::Context context_;
  proto->context_proto(session_id, &context_);
  print_context(context_);
}

inline static bool is_prefix_of(const std::string& str,
                                const std::string& prefix) {
  return std::mismatch(prefix.begin(),
                       prefix.end(),
                       str.begin()).first == prefix.end();
}

bool execute_special_command(const std::string& line,
                             RimeSessionId session_id) {
  RimeApi* rime = rime_get_api();
  if (line == "print schema list") {
    RimeSchemaList list;
    if (rime->get_schema_list(&list)) {
      std::cout << "schema list:" << std::endl;
      for (size_t i = 0; i < list.size; ++i) {
        std::cout << (i + 1) << ". " << list.list[i].name
                  << " [" << list.list[i].schema_id << "]" << std::endl;
      }
      rime->free_schema_list(&list);
    }
    char current[100] = {0};
    if (rime->get_current_schema(session_id, current, sizeof(current))) {
      std::cout << "current schema: [" << current << "]" << std::endl;
    }
    return true;
  }
  const std::string kSelectSchema = "select schema ";
  if (is_prefix_of(line, kSelectSchema)) {
    auto schema_id = line.substr(kSelectSchema.length());
    if (rime->select_schema(session_id, schema_id.c_str())) {
      std::cout << "selected schema: [" << schema_id << "]" << std::endl;
    }
    return true;
  }
  const std::string kSelectCandidate = "select candidate ";
  if (is_prefix_of(line, kSelectCandidate)) {
    int index = std::stoi(line.substr(kSelectCandidate.length()));
    if (index > 0 &&
        rime->select_candidate_on_current_page(session_id, index - 1)) {
      print(session_id);
    } else {
      std::cerr << "cannot select candidate at index " << index << "."
                << std::endl;
    }
    return true;
  }
  if (line == "print candidate list") {
    RimeCandidateListIterator iterator = {0};
    if (rime->candidate_list_begin(session_id, &iterator)) {
      while (rime->candidate_list_next(&iterator)) {
        std::cout << (iterator.index + 1) << ". " << iterator.candidate.text;
        if (iterator.candidate.comment)
          std::cout << " (" << iterator.candidate.comment << ")";
        std::cout << std::endl;
      }
      rime->candidate_list_end(&iterator);
    } else {
      std::cout << "no candidates." << std::endl;
    }
    return true;
  }
  const std::string kSetOption = "set option ";
  if (is_prefix_of(line, kSetOption)) {
    Bool is_on = True;
    auto iter = line.begin() + kSetOption.length();
    const auto end = line.end();
    if (iter != end && *iter == '!') {
      is_on = False;
      ++iter;
    }
    auto option = std::string(iter, end);
    rime->set_option(session_id, option.c_str(), is_on);
    std::cout << option << " set " << (is_on ? "on" : "off") << std::endl;
    return true;
  }
  return false;
}

void on_message(void* context_object,
                RimeSessionId session_id,
                const char* message_type,
                const char* message_value) {
  std::cout << "message: [" << session_id << "] [" << message_type << "] "
            << message_value << std::endl;
}

int main(int argc, char *argv[]) {
  RimeApi* rime = rime_get_api();

  RIME_STRUCT(RimeTraits, traits);
  traits.app_name = "rime.console";
  rime->setup(&traits);

  rime->set_notification_handler(&on_message, NULL);

  std::cerr << "initializing..." << std::endl;
  rime->initialize(NULL);
  Bool full_check = True;
  if (rime->start_maintenance(full_check))
    rime->join_maintenance_thread();
  std::cerr << "ready." << std::endl;

  RimeSessionId session_id = rime->create_session();
  if (!session_id) {
    std::cerr << "Error creating rime session." << std::endl;
    return 1;
  }

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "exit")
      break;
    if (execute_special_command(line, session_id))
      continue;
    if (rime->simulate_key_sequence(session_id, line.c_str())) {
      print(session_id);
    } else {
      std::cerr << "Error processing key sequence: " << line << std::endl;
    }
  }

  rime->destroy_session(session_id);
  rime->finalize();
  return 0;
}
