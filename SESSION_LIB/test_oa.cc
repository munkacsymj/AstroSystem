#include "observing_action.h"
#include "session.h"
#include <julian.h>
#include <string>
#include <list>
#include <time.h>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  string s("v1463-her,TimeSeq(Pri),TimeSeq(Holes),Script()");
  list<string> l_of_s;

  l_of_s.push_back(s);

  list<ObservingAction *> answer;

  time_t now = time(0);
  JULIAN jd_start(2459215.268808);
  JULIAN jd_end = jd_start.add_days(0.5);

  SessionOptions opts;
  opts.no_session_file = 1;


  Session session(jd_start, "/home/mark/ASTRO/NEWCAMERA/SESSION_LIB/session.txt", opts);
  Strategy::FindAllStrategies(&session);
  Strategy::BuildObservingActions(&session);
  PrintSummaryByGroups();

  Schedule *session_schedule = session.SessionSchedule();

  session_schedule->set_start_time(jd_start);
  session_schedule->set_finish_time(jd_end);
  session_schedule->initialize_schedule();
  session_schedule->create_schedule();

  //Strategy strategy("st-sgr", 0);

  //ObservingAction::Factory(l_of_s, answer, strategy, session);

  //std::cout << "answer contains " << answer.size() << " elements." << std::endl;
  //for (ObservingAction *oa : answer) {
  //std::cout << *oa << std::endl;
  //}
}
