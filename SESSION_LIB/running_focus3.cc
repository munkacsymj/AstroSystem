/*  running_focus.cc -- (Current) Hyperbola-matching to predict point of best focus
 *
 *  Copyright (C) 2015, 2018, 2023 Mark J. Munkacsy
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program (file: COPYING).  If not, see
 *   <http://www.gnu.org/licenses/>. 
 */

//****************************************************************
//        Preprocessor Story
//
// There are three "modes" for which this can be compiled:
//    1. For nomal (live) running with time_seq
//    2. To "replay" data recorded during a time_seq session
//    3. With a focus simulator
//
// In Mode 1:
//    symbol LIVE is defined
//    symbol MOVE_FOCUS is defined
// In Mode 2:
//    LIVE is not defined
//    REPLAY is defined
//    SIM is not defined
//    STANDALONE is defined
//    MOVE_FOCUS is not defined
// In Mode 3:
//    LIVE is not defined
//    REPLAY is not defined
//    SIM is defined
//    STANDALONE is defined
//    MOVE_FOCUS is defined
//
// Behavior preprocessor stuff:
//    CONTINUOUS_DITHER
//****************************************************************

//#define CONTINUOUS_DITHER

#define LIVE                 // use the realtime clock
//#define REPLAY               // use an input file
//#define SIM                    // use focus_simulator
//#define STANDALONE             // create a main()
#define MOVE_FOCUS             // issue focus commands (might be sim)

#include <stdio.h>
#include <math.h>		// for fabs()
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_permutation.h>
#include <gsl/gsl_linalg.h>
#include "running_focus.h"
#include "running_focus3_int.h"
#include "gaussian_fit.h"
#include "scope_api.h"
#include "system_config.h"
#include <assert.h>
#include <list>
#include <vector>
#include <ceres/ceres.h>
#include <glog/logging.h>
#ifdef SIM
#include "focus_simulator.h"
#endif // SIM

#ifdef SIM
static long sim_focuser_ticks;
static double sim_time;
#endif

double gaussian(Image *image, int *status);
void PrintMeasurements(void);

//****************************************************************
//        Configuration constants
//****************************************************************

static double f_number(void) {
  static SystemConfig config;
  return config.FocalRatio();
}

static double focus_slope(void) {
  static SystemConfig config;
  return config.FocusSlope(FOCUSER_FINE); // based on bin 3x3??
}

//static double M = 0.0135;	// asymptotic slope (75)
//static double M = 0.010753*10.0/f_number();	// asymptotic slope (93)
static constexpr int bin_factor = 3; // for binning 3x3
double M = bin_factor/focus_slope();

//****************************************************************
//        Globals
//****************************************************************

struct Measurement {
  double ticks;
  double blur;
  JULIAN raw_time;
  double offset;
  double delta_t;
};

static struct {
  std::vector<Measurement *> measurements;
  double ref_time;
  double smallest_offset{9.9e99};
  double biggest_offset;
  std::list<double> last_5_blurs;
} context;

//****************************************************************
//        class FittingParams
//****************************************************************
enum FP_ID {
  FP_C0,			// C0 first segment
  FP_A0,			// A0 first segment
  FP_R0,			// R first segment
  FP_R1,			// R second segment
};

template <typename E, class T, std::size_t N>
class enum_array : public std::array<T,N> {
public:
  T & operator[] (E e) {
    return std::array<T,N>::operator[]((std::size_t)e);
  }
  const T & operator[] (E e) const {
    return std::array<T,N>::operator[]((std::size_t)e);
  }
};

class FittingParams {
public:
  void SetVariable(FP_ID param);
  bool IsVariable(FP_ID param) const { return is_variable[param]; }
  void SmartAssign0(FocusModelState &tgt, const FocusModelState &src) const ;
  void SmartAssign1(FocusModelState &tgt, const FocusModelState &src) const ;
  void LoadIntoParamArray(double *array,
			  const FocusModelState *src0,
			  const FocusModelState *src1 = nullptr);
  void FetchFromParamArray(FocusModelState *tgt0, FocusModelState *tgt1, const double *array) const;
  int NumVariables(void) const;
  void Reset(void);
  int ParamIndex(FP_ID id) const { return param_index[id]; }

private:
  enum_array<FP_ID, bool, 4> is_variable{false};
  enum_array<FP_ID, int, 4> param_index;
  void SetupParamArray(void);
};

void
FittingParams::Reset(void) {
  for (auto &x : is_variable) {
    x = false;
  }
  for (auto &x : param_index) {
    x = -1; // invalid
  }
}
  

int
FittingParams::NumVariables(void) const {
  int count = 0;
  for (const bool &x : this->is_variable) {
    if(x) count++;
  }
  return count;
}

void
FittingParams::SetVariable(FP_ID param) {
  is_variable[param] = true;
}

void
FittingParams::SmartAssign0(FocusModelState &tgt, const FocusModelState &src) const {
  if (is_variable[FP_C0]) tgt.C = src.C;
  if (is_variable[FP_A0]) tgt.A = src.A;
  if (is_variable[FP_R0]) tgt.R = src.R;
}
void
FittingParams::SmartAssign1(FocusModelState &tgt, const FocusModelState &src) const {
  if (is_variable[FP_R1]) tgt.R = src.R;
}
  
void
FittingParams::LoadIntoParamArray(double *array,
				  const FocusModelState *src0,
				  const FocusModelState *src1) {
  SetupParamArray();
  if (is_variable[FP_C0]) {
    array[param_index[FP_C0]] = src0->C;
  }
  if (is_variable[FP_R0]) {
    array[param_index[FP_R0]] = src0->R;
  }
  if (is_variable[FP_A0]) {
    array[param_index[FP_A0]] = src0->A;
  }
  if (is_variable[FP_R1]) {
    assert(src1);
    array[param_index[FP_R1]] = src1->R;
  }
}
  
void
FittingParams::SetupParamArray(void) {
  int tgt = 0;
  if (is_variable[FP_C0]) {
    param_index[FP_C0] = tgt++;
  }
  if (is_variable[FP_R0]) {
    param_index[FP_R0] = tgt++;
  }
  if (is_variable[FP_A0]) {
    param_index[FP_A0] = tgt++;
  }
  if (is_variable[FP_R1]) {
    param_index[FP_R1] = tgt++;
  }
}

void
FittingParams::FetchFromParamArray(FocusModelState *tgt0, FocusModelState *tgt1, const double *array) const {
  //SetupParamArray();
  if (is_variable[FP_C0]) tgt0->C = array[param_index[FP_C0]];
  if (is_variable[FP_R0]) tgt0->R = array[param_index[FP_R0]];
  if (is_variable[FP_A0]) tgt0->A = array[param_index[FP_A0]];
  if (is_variable[FP_R1]) tgt1->R = array[param_index[FP_R1]];
}
  

//****************************************************************
//        CompositeModel
//****************************************************************
int MaxNumSegmentsForOffsetTime() {
  const double &offset = context.measurements.back()->offset;
  constexpr double nominal_time = 1300.0;
  if (offset < nominal_time) return 1;
  if (offset < 2*nominal_time) return 2;
  return 1 + (int) 0.5+offset/(2*nominal_time);
}

enum ModelMode { M_FIXED, M_FLAT, M_NORMAL };

struct Segment {
  double start_t_offset;
  double end_t_offset;
  HypFocusModel *model;
};

class CompositeModel {
public:
  CompositeModel(ModelMode mode, CompositeModel *parent);
  CompositeModel(ModelMode mode, HypFocusModel *parent,
		 double start_offset=0.0, double end_offset=0.0);
  CompositeModel(int num_segments);
  ~CompositeModel(void);

  int NumPointsInFinalSegment(void) const;
  int NumFittingParams(void) const;
  void FixConstraints(void);
  void SplitFinalSegment(void);
  const Segment *MatchingSegment(double offset_t) const;
  double PredictBlur(double offset_t, double ticks) const;
  double BestFocus(double offset_t) const;
  double GetSumSqResiduals(void) const;
  void Recalculate(void);	// effectively, a "TailPairSolve()"
  double CalculateAIC(void) const;
  void RefreshFinalSegmentBound(double final_offset);
  void PrintSummary(FILE *fp) const;
  void ChangeMode(ModelMode new_mode);
  bool SolutionIsCredible(void) const;
  int NumSegments(void) const { return (int) this->segments.size(); }
  void DoPairOptimize(const HypFocusModel *init_model,
		      const Segment &s1,
		      FittingParams &fitting,
		      const Segment &s2);
  void ChainPairSolve(int start_segment=0);
  void SingleModelSolve(int segment, FittingParams &fitting);

  std::string origin;
  int seq_no;
  FittingParams fitting_params;
  std::vector<Segment> segments;
  double residual_sumsq;

private:
  ModelMode model_mode;
};
  
static int model_seq_no = 0;

CompositeModel::CompositeModel(int num_segments) {
  this->model_mode = M_NORMAL;
  this->seq_no = model_seq_no++;
  double segment_timespan = (context.biggest_offset - context.smallest_offset)/num_segments;
  double this_segment_start_t = context.smallest_offset;

  for (int i=0; i<num_segments; i++) {
    Segment seg;
    seg.model = new HypFocusModel();
    seg.start_t_offset = this_segment_start_t;
    seg.end_t_offset = seg.start_t_offset + segment_timespan;
    seg.model->SetOffsets(seg.start_t_offset, seg.end_t_offset);
    this_segment_start_t = seg.end_t_offset;
    this->segments.push_back(seg);
  }
  Segment &end_seg = this->segments.back();
  end_seg.end_t_offset = context.biggest_offset;
  end_seg.model->SetOffsets(end_seg.start_t_offset, end_seg.end_t_offset);
}

CompositeModel::CompositeModel(ModelMode mode, CompositeModel *parent) {
  this->model_mode = mode;
  this->seq_no = model_seq_no++;
  
  for (Segment &s : parent->segments) {
    this->segments.push_back(s);
    Segment &newseg = this->segments.back();
    newseg.model = s.model->DeepCopy();
  }
}

CompositeModel::CompositeModel(ModelMode mode, HypFocusModel *parent,
			       double start_offset, double end_offset) {
  this->model_mode = mode;
  this->seq_no = model_seq_no++;

  this->segments.push_back(Segment({start_offset, end_offset, parent}));
  parent->SetOffsets(start_offset, end_offset);
}

CompositeModel::~CompositeModel(void) {
  for (Segment &seg : this->segments) {
    fprintf(stderr, "(E) Deleting model at %p\n", (void *) seg.model);
    delete seg.model;
  }
  this->segments.clear();
  fprintf(stderr, "Destroying composite model %d\n", this->seq_no);
}

const Segment *
CompositeModel::MatchingSegment(double offset_t) const {
  for (const Segment &seg : this->segments) {
    if (offset_t >= seg.start_t_offset and offset_t <= seg.end_t_offset) {
      return &seg;
    }
  }
  fprintf(stderr, "MatchingSegment: failed to find offset_t = %lf\n", offset_t);
  return nullptr;
}

double
CompositeModel::BestFocus(double offset_t) const {
  const Segment &last = this->segments.back();
  return last.model->BestFocus(offset_t);
}

void
CompositeModel::FixConstraints(void) {
  for (Segment &seg : this->segments) {
    seg.model->SetConstrained(true);
  }
  if (this->segments.size() == 1 or
      (this->segments.size() == 2 and this->model_mode == M_NORMAL)) {
    this->segments.front().model->SetConstrained(true);
  }
}
  
void
CompositeModel::SplitFinalSegment(void) {
  Segment &final_seg = this->segments.back();
  Segment new_seg;
  new_seg.start_t_offset = (final_seg.start_t_offset + final_seg.end_t_offset)/2.0;
  new_seg.end_t_offset = final_seg.end_t_offset;
  final_seg.end_t_offset = new_seg.start_t_offset;
  new_seg.model = new HypFocusModel();
  new_seg.model->SetInitialConditions(FocusModelState({
	final_seg.model->BestFocus(new_seg.start_t_offset), // C
	0.0,						    // R
	final_seg.model->AValue(new_seg.start_t_offset),    // A
	0.0,						    // AR
	new_seg.start_t_offset}));			    // t0
  this->segments.push_back(new_seg);
  new_seg.model->SetOffsets(new_seg.start_t_offset, new_seg.end_t_offset);
  this->FixConstraints();
}


void
CompositeModel::RefreshFinalSegmentBound(double final_offset) {
  this->segments.back().end_t_offset = final_offset;
}

void CompositeModel::Recalculate(void) {
  this->RefreshFinalSegmentBound(context.measurements.back()->offset);

  switch (this->model_mode) {
  case M_FIXED:
    // do nothing
    break;
    
  case M_FLAT:
    assert(this->segments.size() == 1);
    this->fitting_params.Reset();
    this->fitting_params.SetVariable(FP_C0);
    this->fitting_params.SetVariable(FP_A0);
    this->SingleModelSolve(0, this->fitting_params);
    break;
    
  case M_NORMAL:
    if (this->segments.size() > 1) {
      this->ChainPairSolve();
    } else {
      this->SingleModelSolve(0, this->fitting_params);
    }
    break;

  default:
    fprintf(stderr, "CompositeModel: invalid model_mode: %d\n",
	    this->model_mode);
  }
  this->residual_sumsq = this->GetSumSqResiduals();
  this->PrintSummary(stdout);
}

double CompositeModel::PredictBlur(double offset_t, double ticks) const {
  const Segment *seg = this->MatchingSegment(offset_t);
  if (seg) {
    return seg->model->PredictBlur(offset_t, ticks);
  } else {
    fprintf(stderr, "CompositeModel: GetPrediction: no segment found for offset %.0lf\n",
	    offset_t);
  }
  return -1.0;
}

void
CompositeModel::ChangeMode(ModelMode new_mode) {
  model_mode = new_mode;
}

int CompositeModel::NumFittingParams(void) const {
  int num_seg = (int) this->segments.size();
  if (num_seg == 1) return 2;
  return 3*num_seg;
}
  
double
CompositeModel::GetSumSqResiduals(void) const {
  double sum = 0.0;

  for (Measurement *m : context.measurements) {
    double residual = this->PredictBlur(m->offset, m->ticks) - m->blur;
    sum += (residual*residual);
  }
  return sum;
}

bool StateVectorIsCredible(const FocusModelState &state) {
  const double abs_R = fabs(state.R);
  const double abs_AR = fabs(state.AR);
  bool credibility = (state.C > 0 and state.C < 420000 and
		      //state.A > 0.4 and // seemed to arbitrarily exclude good solutions
		      state.A < 2.5 and
		      abs_R < (400000/3600.0) and
		      abs_AR < (10.0/3600.0));
  if (credibility == false) {
    std::cerr << "Credibility check fail: \n";
    std::cerr << "   state.C = " << state.C << ",  ";
    std::cerr << "   state.A = " << state.A << '\n';
    std::cerr << "   abs_R = " << abs_R << ", abs_AR = " << abs_AR << '\n';
  }
  return credibility;
}

bool
CompositeModel::SolutionIsCredible(void) const {
  for (const Segment &s : this->segments) {
    HypFocusModel *m = s.model;
    if ((not StateVectorIsCredible(m->GetStateVector(s.start_t_offset))) or
	(not StateVectorIsCredible(m->GetStateVector(s.end_t_offset)))) {
      return false;
    }
  }
  return true;
}

int
CompositeModel::NumPointsInFinalSegment(void) const {
  HypFocusModel *m = this->segments.back().model;
  return m->NumPointsInSubset();
}

void
CompositeModel::PrintSummary(FILE *fp) const {
  fprintf(fp, "CompositeModel(%d), [%s], %s:\n",
	  seq_no, origin.c_str(),
	  (model_mode == M_FIXED ? "FIXED" : (model_mode == M_FLAT ? "FLAT" : "NORMAL")));
  for (const Segment &s : segments) {
    const FocusModelState &state = s.model->GetStateVector(s.start_t_offset);
    fprintf(fp, "    seg [%.1lf - %.1lf] C=%.0lf, R=%lf, A=%.4lf, AR=%lf, t0=%.1lf\n",
	    s.start_t_offset,
	    s.end_t_offset,
	    state.C,
	    state.R,
	    state.A,
	    state.AR,
	    state.t0);
  }
}

//****************************************************************
//        HypFocusModel
//****************************************************************

int
HypFocusModel::NumFittingParams(void) {
  assert(0 && "Illegal call to HypFocusModel::NumFittingParams"); // should never be called
  return (this->constrained ? 2 : 4);
}

double
HypFocusModel::PredictBlur(double offset_t, double ticks) {
  const double del_t = offset_t - this->int_state.t0;
  const double delta_ticks = ticks - (this->int_state.C + del_t*this->int_state.R);
  const double t = (delta_ticks * M);
  const double a0 = this->int_state.A + del_t*this->int_state.AR;
  return sqrt(a0*a0 + t*t);
}

HypFocusModel::HypFocusModel(void) {
  this->constrained = false;
  int_state = {
    context.measurements.front()->ticks, // C
	  0.0,				       // R
	  1.0,				       // A
	  0.0,				       // AR
	  context.smallest_offset	       // t0
  };
}

HypFocusModel *
HypFocusModel::DeepCopy(void) {
  HypFocusModel *m = new HypFocusModel;
  m->int_state = this->int_state;
  m->constrained = this->constrained;
  m->offset_start_ = this->offset_start_;
  m->offset_end_ = this->offset_start_;
  return m;
}
	  
double
HypFocusModel::AValue(double offset_t) const {
  return this->int_state.A + (offset_t - this->int_state.t0)*this->int_state.AR;
}

FocusModelState
HypFocusModel::GetStateVector(double offset_t) const {
  return FocusModelState({
      BestFocus(offset_t),	// C
      this->int_state.R,	// R
      AValue(offset_t),		// A
      this->int_state.AR,	// AR
      this->int_state.t0	// t0
    });
}

void
HypFocusModel::SetInitialConditions(const FocusModelState &init) {
  this->int_state = init;
}

double
HypFocusModel::BestFocus(double offset_t) const {
  return this->int_state.C + (offset_t - this->int_state.t0)*this->int_state.R;
}

double
HypFocusModel::GetSumSqResiduals(void) {
  this->RefreshSubset();
  double sumsq = 0.0;
  for(Measurement *m : this->subset) {
    const double residual = m->blur - this->PredictBlur(m->offset, m->ticks);
    sumsq += (residual*residual);
  }
  return sumsq;
}

int
HypFocusModel::NumPointsInSubset(void) {
  this->RefreshSubset();
  return this->subset.size();
}

void
HypFocusModel::RefreshSubset(void) {
  this->subset.clear();
  for (Measurement *m : context.measurements) {
    if (m->offset >= offset_start_ and
	m->offset <= offset_end_) {
      this->subset.push_back(m);
    }
  }
}

void
HypFocusModel::SetOffsets(double start, double end) {
  this->offset_start_ = start;
  this->offset_end_ = end;
}

//****************************************************************
//        Ceres Solver
//****************************************************************
using ceres::AutoDiffCostFunction;
using ceres::CostFunction;
using ceres::Problem;
using ceres::Solve;
using ceres::Solver;

class CostFunctionPairN : public ceres::CostFunction {
public:
  CostFunctionPairN(const FittingParams &fitting_params,
		    bool second_half,
		    const FocusModelState *state1,
		    const FocusModelState *state2,
		    const Measurement *m) : m_(m), state1_(state1), state2_(state2),
					    num_param_(fitting_params.NumVariables()),
					    fitting_params_(fitting_params),
					    second_half_(second_half) {SetParamSizes();}
  virtual ~CostFunctionPairN(void) {}
  void SetParamSizes(void) {
    this->set_num_residuals(1);	// just "blur"
    param_lengths[0] = fitting_params_.NumVariables();
    *this->mutable_parameter_block_sizes() = param_lengths;
  }
  virtual bool Evaluate(double const * const * parameters,
			double * residuals,
			double ** jacobians) const {
    FocusModelState s1 = *state1_;
    FocusModelState s2;
    if (state2_) s2 = *state2_;
    fitting_params_.FetchFromParamArray(&s1, &s2, parameters[0]);
    
    const double &C1 = s1.C;
    const double &R1 = s1.R;
    const double &A1 = s1.A;
    const double &AR1 = 0.0;
    const double &R2 = s2.R;
    const double &AR2 = 0.0;
    const double &t = m_->delta_t; // in its respective subset
    double a_term, c_term;
    if (second_half_) {
      a_term = A1 + AR1*(s2.t0-s1.t0) + AR2*t;
      c_term = m_->ticks - (C1 + R1*(s2.t0-s1.t0) + R2*t);
    } else {
      a_term = A1 + t*AR1;
      c_term = m_->ticks - (C1 + t*R1);
    }
    const double blur = sqrt(a_term*a_term + M*M*c_term*c_term);
    residuals[0] = blur - m_->blur;

    if (jacobians != nullptr and jacobians[0] != nullptr) {
      if (fitting_params_.IsVariable(FP_C0)) {
	jacobians[0][fitting_params_.ParamIndex(FP_C0)] =
	  -M*M*c_term/blur;	// C1
      }
      if (fitting_params_.IsVariable(FP_A0)) {
	jacobians[0][fitting_params_.ParamIndex(FP_A0)] =
	  a_term/blur;				  // A1
      }
      if (fitting_params_.IsVariable(FP_R0)) {
	if (second_half_) {
	  jacobians[0][fitting_params_.ParamIndex(FP_R0)] =
	    -M*M*(s2.t0-s1.t0)*c_term/blur; // R1
	} else {
	  jacobians[0][fitting_params_.ParamIndex(FP_R0)] =
	    -M*M*t*(c_term)/blur; // R1
	}
      }
      if (fitting_params_.IsVariable(FP_R1)) {
	if (second_half_) {
	  jacobians[0][fitting_params_.ParamIndex(FP_R1)] =
	    -M*M*t*(c_term)/blur;
	} else {
	  jacobians[0][fitting_params_.ParamIndex(FP_R1)] = 0.0;
	}
      }
    }
    return true;
  }
    
private:
  std::vector<int32_t> param_lengths{0};
  const Measurement *m_;
  const FocusModelState *state1_;
  const FocusModelState *state2_;
  int num_param_;
  const FittingParams fitting_params_;
  bool second_half_;
};

//****************************************************************
//        Recalculate()
//****************************************************************
void
CompositeModel::ChainPairSolve(int start_segment) {
  while(start_segment+1 < this->NumSegments()) {
    FittingParams fitting;
    const HypFocusModel *init_model = nullptr;
    if (start_segment == 0) {
      fitting.SetVariable(FP_C0);
      fitting.SetVariable(FP_A0);
    } else {
      init_model = this->segments[start_segment-1].model;
    }
    fitting.SetVariable(FP_R0);
    fitting.SetVariable(FP_R1);

    this->DoPairOptimize(init_model,
			 this->segments[start_segment],
			 fitting,
			 this->segments[start_segment+1]);

    start_segment++;
  }
}

void
CompositeModel::SingleModelSolve(int segment,
				 FittingParams &fitting) {
  FocusModelState init_state {
    context.measurements.front()->ticks, // C
    0.0,				 // R
    1.5,				 // A
    0.0,				 // AR
    context.measurements.front()->offset }; // t0
  Problem problem;
  Solver::Options options;
  options.max_num_iterations = 25;
  options.linear_solver_type = ceres::DENSE_QR;
  options.minimizer_progress_to_stdout = false;

  Solver::Summary summary;

  double params[4];
  fitting.LoadIntoParamArray(params, &init_state);

  const Segment &seg = this->segments[segment];
  for (Measurement *m : context.measurements) {
    if (m->offset >= seg.start_t_offset and
	m->offset <= seg.end_t_offset) {
      m->delta_t = m->offset - seg.start_t_offset;

      CostFunctionPairN *cost_function =
	new CostFunctionPairN(fitting,
			      false, // first half
			      &init_state,
			      nullptr, // no second segment state
			      m);

      problem.AddResidualBlock(cost_function, nullptr, params);
    }
  }
  Solve(options, &problem, &summary);
  std::cout << "SingleModelSolve:\n";
  std::cout << summary.FullReport() << "\n";
  std::cout.flush();

  FocusModelState final_state{init_state};
  fitting.FetchFromParamArray(&final_state, nullptr, params);

  this->segments[segment].model->SetInitialConditions(final_state);
}

void
CompositeModel::DoPairOptimize(const HypFocusModel *init_model, // might be nullptr
			       const Segment &prior_seg,
			       FittingParams &fitting,
			       const Segment &last_seg) {
  HypFocusModel *last_model = last_seg.model;
  HypFocusModel *prior_model = prior_seg.model;

  FocusModelState state1;
  FocusModelState state2(last_model->GetStateVector(last_seg.start_t_offset));
  state2.t0 = last_seg.start_t_offset;
  
  if (init_model and not fitting.IsVariable(FP_C0)) {
    state1 = init_model->GetStateVector(prior_seg.start_t_offset);
  } else {
    state1 = prior_model->GetStateVector(prior_seg.start_t_offset);
  }
  state1.t0 = prior_seg.start_t_offset;

  last_model->RefreshSubset();
  prior_model->RefreshSubset();
  fprintf(stderr, "DoPairOptimize() starting with %d/%d points.\n",
	  (int) prior_model->subset.size(),
	  (int) last_model->subset.size());

  Problem problem;
  Solver::Options options;
  options.max_num_iterations = 25;
  options.linear_solver_type = ceres::DENSE_QR;
  options.minimizer_progress_to_stdout = true;

  Solver::Summary summary;

  double params[8];
  fitting.LoadIntoParamArray(params, &state1, &state2);

  for (Measurement *m : prior_model->subset) {
    m->delta_t = m->offset - prior_model->int_state.t0;

    CostFunctionPairN *cost_function =
      new CostFunctionPairN(fitting,
			    false, // first half
			    &state1,
			    &state2,
			    m);
    problem.AddResidualBlock(cost_function, nullptr, params);
  }

  for (Measurement *m : last_model->subset) {
    m->delta_t = m->offset - last_model->int_state.t0;
    CostFunctionPairN *cost_function =
      new CostFunctionPairN(fitting,
			    true, // second half
			    &state1,
			    &state2,
			    m);
    problem.AddResidualBlock(cost_function, nullptr, params);
  }
    
  Solve(options, &problem, &summary);
  std::cout << summary.FullReport() << "\n";
  std::cout.flush();

  FocusModelState prior_model_state{state1};
  FocusModelState last_model_state{state2};
  fitting.FetchFromParamArray(&prior_model_state, &last_model_state, params);

  prior_seg.model->SetInitialConditions(prior_model_state);
  last_model_state.C = prior_model_state.C + prior_model_state.R*(last_seg.start_t_offset-
								  prior_seg.start_t_offset);
  last_model_state.A = prior_model_state.A;
  last_seg.model->SetInitialConditions(last_model_state);
}


//****************************************************************
//        AIC stuff
//****************************************************************
double
CompositeModel::CalculateAIC(void) const {
  const long N = context.measurements.size();
  const int K = this->NumFittingParams()+1;
  const double sumsq = this->GetSumSqResiduals();
  return N*log(sumsq/N) + 2*K + (2*K*(K+1)/(N-K-1));
}

double
ProbOfDifference(double aic1, double aic2) {
  const double AIC_diff = fabs(aic1-aic2);
  const double term = exp(0.5*AIC_diff);
  return term/(1.0+term);
}

//****************************************************************
//        RunningFocus
//****************************************************************

const char *
RunningFocus::current_time_string(void) {
  time_t now = time(0);
  struct tm time_info;
  localtime_r(&now, &time_info);
  static char time_string[132];
  sprintf(time_string, "%02d:%02d:%02d",
	  time_info.tm_hour,
	  time_info.tm_min,
	  time_info.tm_sec);
  return time_string;
}

RunningFocus::RunningFocus(const char *logfilename) {
  ref_model = nullptr;
  measurements_still_to_ignore = 3;
  rf_log_file_name = logfilename;
  rf_log_file = fopen(logfilename, "w");
  time_origin = time(0);
  dither_counter = -1;
  initial_focus = CumFocusPosition();
  fprintf(rf_log_file, "%s: RunningFocus initialized.\n", current_time_string());
  //scope_focus(0); // initialize handlers in scope_api
  fprintf(stderr, "Using M = %lf\n", M);
}

void
RunningFocus::ClearFittingModels(void) {
  if (this->ref_model) {
    fprintf(stderr, "(B) Deleting model at %p\n", (void *) this->ref_model);
    delete this->ref_model;
    this->ref_model = nullptr;
  }
  for (auto m : this->fitting_models) {
    fprintf(stderr, "(C) Deleting model at %p\n", (void *) m);
    delete m;
  }
  this->fitting_models.clear();
}

RunningFocus::~RunningFocus(void) {
  fprintf(rf_log_file, "%s: RunningFocus destructor inovoked.\n", current_time_string());
  if (this->ref_model) {
    fprintf(stderr, "(D) Deleting model at %p\n", (void *) this->ref_model);
    delete this->ref_model;
    this->ref_model = nullptr;
  }
  this->ClearFittingModels();
  fclose(rf_log_file);
}

void
RunningFocus::Restart(void) {
  fprintf(rf_log_file, "%s: RunningFocus: Restart!\n", current_time_string());
  time_origin = time(0);
  dither_counter = -1;
  measurements_still_to_ignore = 3;
  if (this->ref_model) {
    delete this->ref_model;
    ref_model = nullptr;
  }
  this->ClearFittingModels();
  context.measurements.clear();
  context.last_5_blurs.clear();
}

void
RunningFocus::PerformFocusDither(void) {
  dither_counter = 0;
}

void
RunningFocus::SetInitialImagesToIgnore(int num_to_ignore) {
  measurements_still_to_ignore = num_to_ignore;
}

//****************************************************************
//        RunningFocus::AddImage()
//****************************************************************
void
RunningFocus::AddImage(const char *image_filename) {
  Image image(image_filename);
  AddImage(&image);
}

void
RunningFocus::AddImage(Image *image) {
  // only create a FocusMeasurement if this image yields a valid
  // measurement.

#ifdef LIVE
  int status = 0;
  ImageInfo *info = image->GetImageInfo();
  IStarList *star_list = image->GetIStarList();
  CompositeImage *composite = BuildComposite(image, star_list);
  double gaussian_value = gaussian(composite, &status);

  ALT_AZ loc = info->GetAzEl();
  const double zenith_angle = M_PI/2.0 - loc.altitude_of();
  // This equation for blur_factor comes from princeton.edu, course
  // material for AST542 (Cristobal Petrovich) 
  const double blur_factor = pow(cos(zenith_angle), 0.6);
  if (!isnan(blur_factor)) {
    //if (info->AirmassValid()) {
    //gaussian_value -= (info->GetAirmass() - 1.0)*0.255;
    //} else {
    //fprintf(stderr, "WARNING: missing airmass will affect focus!\n");
    //}
    gaussian_value *= blur_factor;
  }
  
  if (status == 0) { // success, ...
    //double delta_t = 3600*24*(info->GetExposureMidpoint() - JULIAN(time_origin));
    //fprintf(rf_log_file, "%s: RunningFocus AddImage()\n", current_time_string());
    AddPoint(gaussian_value, info->GetFocus(), info->GetExposureMidpoint());
  } // end if status was good
#endif
}

void
RunningFocus::AddPoint(double gaussian, double focuser, JULIAN time_tag) {
  static FILE *fp_points = nullptr;
  
  if (measurements_still_to_ignore > 0) {
    fprintf(rf_log_file, "Ignoring measurement (startup).\n");
    measurements_still_to_ignore--;
  } else {
    if (context.measurements.size() == 0) {
      context.ref_time = time_tag.day();
    }
    Measurement *m = new Measurement;
    m->ticks = focuser;
    m->raw_time = time_tag;
    m->blur = gaussian;
    m->offset = (time_tag.day() - context.ref_time)*24.0*3600.0;
    context.biggest_offset = m->offset;
    if (context.smallest_offset > m->offset) context.smallest_offset = m->offset;
    context.measurements.push_back(m);
    fprintf(rf_log_file, "%s   Added point. %s: gaus = %lf, ticks = %.0lf\n",
	    current_time_string(),
	    time_tag.to_string(), gaussian, focuser);
    fflush(rf_log_file);

    //fprintf(stderr, "AddPoint, initial size = %ld...", last_5_blurs.size());
    context.last_5_blurs.push_back(gaussian);
    //fprintf(stderr, " ... size changed to %ld\n", last_5_blurs.size());
    if (context.last_5_blurs.size() > 5) {
      //fprintf(stderr, "AddPoint: size = %ld, popping.\n", last_5_blurs.size());
      context.last_5_blurs.pop_front();
    }

    if (fp_points == nullptr) {
      fp_points = fopen("/tmp/points.csv", "w");
      if (!fp_points) {
	std::cerr << "AddPoint: unable to create points.csv file.\n";
      }
    }
    fprintf(fp_points, "%.0lf,%lf,%.0lf\n",
	    m->ticks, m->blur, m->offset);
    fflush(fp_points);
  }
	  
  PrintMeasurements();
}

// Value of 200 works for the Arduino C14 focuser
// Value of 10000 for the ESATTO
#ifdef CONTINUOUS_DITHER

static int dither_size(void) {
  static SystemConfig config;
  if (config.FineFocuserName() == "ESATTO") {
    return 10000.0;
  } else {
    return 50.0;		// JMI focuser
  }
}

static const int dither_sequence[] = {
  -dither_size(), -dither_size(), -dither_size(),
  dither_size(), dither_size(), dither_size(),
  0 };
static const int num_dithers = (sizeof(dither_sequence)/sizeof(dither_sequence[0]));

double
RunningFocus::DoDither(void) {
  static long orig_focus;

  // dither_factor is a scaling factor. It makes dithers bigger when the focus is bad.
  double sum_blurs = 0.0;
  for (double blur : context.last_5_blurs) {
    sum_blurs += blur;
  }

  // dither_factor kept at -1 when not in dither. This ensures that
  // dither_factor is only set once at the start of a dither.
  static double dither_factor = -1.0;
  if (dither_factor < 0.0) {
    if (context.last_5_blurs.size() > 3) {
      dither_factor = (sum_blurs/context.last_5_blurs.size());
    } else {
      dither_factor = 1.0;
    }
  }
  //const double dither_factor = 1.0;

  if (dither_counter < 0) return 0.0;

  if (dither_counter == 0) {
#ifdef LIVE
    orig_focus = CumFocusPosition();
#else
    orig_focus = sim_focuser_ticks;
#endif
    // We will return here when done
  }

  const int dither_move = (int) (dither_factor*dither_sequence[dither_counter]);
  fprintf(rf_log_file, "Dither update; dither_counter = %d, move = %d, factor = %.2lf\n",
	  dither_counter, dither_move, dither_factor);
  dither_counter++;

  if (dither_counter >= num_dithers) {
    dither_counter = -1;
    dither_factor = -1.0;
    // Return back to orig location
    fprintf(rf_log_file, "%s starting focuser move to end dither.\n", current_time_string());
#ifdef LIVE
    const double current_pos = CumFocusPosition();
#else
    const double current_pos = sim_focuser_ticks;
#endif
    return (orig_focus - current_pos);
  } else {
    return dither_move;
  }
  /*NOTREACHED*/
}

void
RunningFocus::UpdateFocus(void) { // update right now
  static JULIAN last_dither;
  
  if (measurements_still_to_ignore > 0) return;

#ifdef SIM
  double focus_target = sim_focuser_ticks;
#else
  double focus_target = CumFocusPosition();
#endif
  double dither_offset = 0.0;
  
  if (this->fitting_models.size() == 0) {
    // Still in startup mode; don't yet have a reference model. Key
    // question is whether we have enough info to start a fitting
    // model.

    // Why 10? Well, it just kinda sounds good.
    if (context.measurements.size() > 10) {
      // We're at the beginning.
      JULIAN data_start_time;
      HypFocusModel *root_model = new HypFocusModel();
      root_model->SetConstrained(false);
      double start_offset = context.measurements.front()->offset;
      for (const Measurement *m : context.measurements) {
	if (m->offset < start_offset) start_offset = m->offset;
      }
      root_model->SetInitialConditions({
	  context.measurements.front()->ticks, // C
	  0.0,				       // R
	  1.0,				       // A
	  0.0,				       // AR
	  start_offset			       // t0
	});
      CompositeModel *cm = new CompositeModel(M_FLAT, root_model);
      cm->fitting_params.SetVariable(FP_C0);
      cm->fitting_params.SetVariable(FP_A0);
      cm->origin = std::string("I");
      this->fitting_models.push_back(cm);
      
      root_model = new HypFocusModel();
      root_model->SetInitialConditions({
	  context.measurements.front()->ticks, // C
	  0.0,				       // R
	  1.0,				       // A
	  0.0,				       // AR
	  start_offset			       // t0
	});
      cm = new CompositeModel(M_NORMAL, root_model);
      cm->fitting_params.SetVariable(FP_C0);
      cm->fitting_params.SetVariable(FP_A0);
      cm->fitting_params.SetVariable(FP_R0);
      cm->origin = std::string("I");
      this->fitting_models.push_back(cm);
      
      assert(this->ref_model == nullptr);
      this->ref_model = new CompositeModel(M_FLAT, cm);
      this->ref_model->fitting_params.Reset();
      this->ref_model->fitting_params.SetVariable(FP_C0);
      this->ref_model->fitting_params.SetVariable(FP_A0);
      this->ref_model->origin = std::string("I");
    } else {
      // No models exist, so use initial focus
      focus_target = this->initial_focus;
    }
  }

#ifdef MOVE_FOCUS
#ifdef CONTINUOUS_DITHER
  if (dither_counter < 0) {
    dither_counter = 0;
  }
#else
  // how often to dither? Every 20 minutes
  if (JULIAN(time(0)) - last_dither > (0.33/24.0)) {
    last_dither = JULIAN(time(0));
    dither_counter = 0;
  }
#endif // not continuous dither

  dither_offset = DoDither();
#endif // move_focus

  if (this->fitting_models.size() > 0) {
  
  //********************************
  //  adjust
  //********************************
  
    if (context.measurements.size() < 5) return;
  
    MeasurementList subset_list;
    subset_list.min_gaussian = 9.99;

    double final_offset = context.measurements.back()->offset;

    // Every 5 points, add new model
    static constexpr int new_model_interval = 5; // every 5 points, add new model
    if (context.measurements.size() % new_model_interval == 0) {
      CompositeModel *m = new CompositeModel(MaxNumSegmentsForOffsetTime());
      m->fitting_params.Reset();
      m->fitting_params.SetVariable(FP_C0);
      m->fitting_params.SetVariable(FP_A0);
      m->fitting_params.SetVariable(FP_R0);
      if (m->NumSegments() > 1) {
	m->fitting_params.SetVariable(FP_R1);
      }
      this->fitting_models.push_back(m);
      m->ChainPairSolve();	// setup initial solution
      m->origin = "5";
    }

    fprintf(rf_log_file, "%s: UpdateFocus() with %d points and %d active models.\n",
	    current_time_string(),
	    (int) context.measurements.size(),
	    (int) this->fitting_models.size());

    // Update each model and see if one is ready to replace the ref model
    this->ref_model->Recalculate(); // this ensures that all measurements fit in the time span
    CompositeModel *best_model = nullptr;
    double best_prob_switch = 0.0;
    const double ref_aic = this->ref_model->CalculateAIC();
    this->ref_model->PrintSummary(rf_log_file);
    fprintf(rf_log_file, "AIC[ref](%d) = %lf, sumsq = %.4lf\n",
	    this->ref_model->seq_no, ref_aic, this->ref_model->residual_sumsq);
  
    // Recalculate all the models, and see if any fitting model is ready to become new reference
    for (CompositeModel *m : this->fitting_models) {
      m->RefreshFinalSegmentBound(final_offset);
      m->Recalculate();
      const double aic = m->CalculateAIC();
      const double switch_prob = ProbOfDifference(ref_aic, aic);
      m->PrintSummary(rf_log_file);
      fprintf(rf_log_file, "  AIC[fitting](%d) = %lf, %s, prob_of_better = %lf, sumsq = %.4lf\n",
	      m->seq_no, aic, (aic < ref_aic ? "better" : "worse"), switch_prob,
	      m->residual_sumsq);
      if (aic < ref_aic and switch_prob > 0.90) {
	if (m->SolutionIsCredible()) {
	  if (switch_prob > best_prob_switch) {
	    best_prob_switch = switch_prob;
	    best_model = m;
	  }
	} else {
	  fprintf(rf_log_file, "    Model(%d) not credible.\n", m->seq_no);
	}
      }
    }

    // Perform promotion
    if (best_model) {
      // time to promote
      fprintf(rf_log_file, "Promoting fitting_model.\n");
      if (this->ref_model) {
	fprintf(stderr, "(A) Deleting model at %p\n", (void *) this->ref_model);
	delete this->ref_model;
	this->ref_model = nullptr;
      }
      this->fitting_models.remove(best_model);
      this->ClearFittingModels();	// this messes with this->ref_model
      this->ref_model = best_model;
      this->ref_model->ChangeMode(M_FIXED);
      this->ref_model->fitting_params.Reset(); // nothing variable

      // Create new fitting models
      for (int n=1; n<= MaxNumSegmentsForOffsetTime(); n++) {
	CompositeModel *m = new CompositeModel(n);
	m->origin = std::string("P"); // promotion
	m->fitting_params.SetVariable(FP_C0);
	m->fitting_params.SetVariable(FP_A0);
	m->fitting_params.SetVariable(FP_R0);
	if (n > 1) {
	  m->fitting_params.SetVariable(FP_R1);
	}

	m->RefreshFinalSegmentBound(final_offset);
	m->Recalculate();
	
	this->fitting_models.push_back(m);
      }
    } else {
      fprintf(rf_log_file, "Keeping ref_model.\n");
    }

    if (this->ref_model) {
      double current_offset = context.measurements.back()->offset;
      fprintf(rf_log_file, "%.1lf Current best focus = %.1lf, model %d\n",
	      current_offset,
	      this->ref_model->BestFocus(current_offset),
	      this->ref_model->seq_no);
      fprintf(stdout, "%.1lf Current best focus = %.1lf, model %d\n",
	      current_offset,
	      this->ref_model->BestFocus(current_offset),
	      this->ref_model->seq_no);
    }

    // Now calculate best position for *now*
#ifdef LIVE
    JULIAN now(time(0));
    //double current_time = (now - time_origin)*24.0*3600.0;
    //const double delta_t = (current_time - analysis_t0);
    long current_focuser_position = CumFocusPosition();
    const double best_focus_now = this->ref_model->BestFocus(24.0*3600.0*(now.day()-context.ref_time));
#else
    const double delta_t = sim_time;
    long current_focuser_position = sim_focuser_ticks;
    const double best_focus_now = this->ref_model->BestFocus(delta_t);
#endif
    //const double focus_drift = delta_t * focus_rate + delta_t * delta_t * rate_acceleration/2.0;
    fprintf(rf_log_file, "best_focus_now: %lf\n", best_focus_now);

    long focus_change = (int) (best_focus_now - current_focuser_position + 0.5);
  
    const long focus_clamp = (int) (0.5 + dither_size()*5.5);
    if (focus_change > focus_clamp) {
      fprintf(rf_log_file, "focus change clamp from %ld to %ld.\n", focus_change, focus_clamp);
      focus_change = focus_clamp;
    }
    if (focus_change < -focus_clamp) {
      fprintf(rf_log_file, "focus change clamp from %ld to %ld.\n", focus_change, -focus_clamp);
      focus_change = -focus_clamp;
    }
    focus_target = current_focuser_position + focus_change;
  }  // end if models exist

  focus_target += dither_offset;
	    
  // print a log message
  fprintf(rf_log_file, "%s    starting focus change to %.0lf (includes dither of %.0lf)\n", 
	  current_time_string(), focus_target, dither_offset);
  fflush(rf_log_file);

#ifdef LIVE
  scope_focus(focus_target, FOCUSER_MOVE_ABSOLUTE);
#else
  sim_focuser_ticks = focus_target;
#endif
}
//****************************************************************
//        GAUSSIAN Computation
//****************************************************************

double gaussian(Image *image, int *status) {
  double dark_reference_pixel = image->HistogramValue(0.05);

  const double center_x = image->width/2.0;
  const double center_y = image->height/2.0;

  Gaussian g;
  g.reset();			// initialize the gaussian
  GRunData run_data;
  run_data.reset();

  for (int row = 0; row < image->height; row++) {
    for (int col = 0; col < image->width; col++) {
      double value = image->pixel(col, row);
      const double del_x = center_x - (col + 0.5);
      const double del_y = center_y - (row + 0.5);
      //const double r = sqrt(del_x*del_x + del_y*del_y);

      double adj_value = value - dark_reference_pixel;

      run_data.add(del_x, del_y, adj_value);
    }
  }

  double return_value = 0.0;
  
  if (nlls_gaussian(&g, &run_data)) {
    fprintf(stderr, "gaussian: no convergence.\n");
    *status = 1;
    return_value = 0.0;
  } else {
    *status = 0;
    return_value = g.state_var[1]/10.0;
    fprintf(stderr, "gaussian: %.3lf\n", return_value);
  }

  return return_value;
}

void
RunningFocus::BatchSolver(void) {
  FILE *fp = fopen("/tmp/batch.csv", "w");

  const double C_low = 150000.0;
  const double C_high = 270000.0;
  const double C_span = (C_high - C_low);
  const int C_count = 10;
  const double C_incr = C_span/C_count;

  const double A_low = 1.0;
  const double A_high = 3.0;
  const double A_span = (A_high - A_low);
  const int A_count = 5;
  const double A_incr = A_span/A_count;

  const double R_low = -30.0;
  const double R_high = 30.0;
  const double R_span = (R_high - R_low);
  const int R_count = 10;
  const double R_incr = R_span/R_count;
  
  for (double C = C_low; C <= C_high; C += C_incr) {
    for (double A = A_low; A <= A_high; A += A_incr) {
      for (double R = R_low; R <= R_high; R += R_incr) {
	HypFocusModel*m = new HypFocusModel();
	double start_offset = 0.0;
	m->SetConstrained(false);
	FocusModelState root_state{
	  C,
	  R,				 // R
	  A,				 // A
	  0.0,				 // AR
	  0.0 };
	m->SetInitialConditions(root_state);
	//m->Recalculate();
	FocusModelState final_state(m->GetStateVector(start_offset));
	const double residuals = m->GetSumSqResiduals();
	fprintf(fp, "%.0lf,%.4lf,%.4lf,%.0lf,%.4lf,%.4lf,%.4lf\n",
		root_state.C, root_state.R, root_state.A,
		final_state.C, final_state.R, final_state.A,
		residuals);
	delete m;
      }
      fprintf(stderr, "C=%.0lf, A=%.4lf\n", C, A);
    }
  }
  fclose(fp);
}

void PrintMeasurements(void) {
  fprintf(stderr, "------ MEASUREMENTS -----\n");
  for (Measurement *m : context.measurements) {
    fprintf(stderr, "%.0lf/%lf, offset=%.0lf\n",
	    m->ticks,
	    m->blur,
	    m->offset);
  }
  fprintf(stderr, "\n");
}

#ifdef STANDALONE
void usage(void) {
  std::cerr << "usage: running_focus3 -i input_file\n";
  exit(1);
}

int main(int argc, char **argv) {
  int ch;

  google::InitGoogleLogging(argv[0]);

#if 0
  const char *input_file = nullptr;

  while((ch = getopt(argc, argv, "i:")) != -1) {
    switch(ch) {
    case 'i':
      input_file = optarg;
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }
  RunningFocus running_focus("/tmp/runningfocus.log");

  if (input_file == nullptr) usage();
  FILE *input_fp = fopen(input_file, "r");
  if (!input_fp) {
    std::cerr << "Cannot open input file " << input_file << '\n';
    usage();
  }

  char buffer[128];
  int count = 0;
  while(fgets(buffer, sizeof(buffer), input_fp)) { // and count++ < 48) {
    long ticks;
    double blur;
    long offset;
    if (sscanf(buffer, "%ld,%lf,%ld", &ticks, &blur, &offset) != 3) {
      std::cerr << "Bad input line: " << buffer << '\n';
    } else {
      JULIAN raw_time(2459000.0+(offset/(24.0*3600.0)));

      running_focus.AddPoint(blur, ticks, raw_time);
      running_focus.UpdateFocus();
    }
  }
  //running_focus.UpdateFocus();

  return 0;
#else
  std::list<std::pair<std::string, std::string>> sim_args;
  char str[2];
  
  while((ch = getopt(argc, argv, "F:C:L:P:S:A:R:")) != -1) {
    switch(ch) {
    case 'C':
    case 'L':
    case 'P':
    case 'S':
    case 'A':
    case 'R':
      str[0] = ch;
      str[1] = '\0';
      sim_args.push_back(std::pair<std::string,std::string>(str, optarg));
      break;

    case 'F':
      sim_focuser_ticks = std::stod(optarg);
      break;

    case '?':
    default:
      usage();
      /*NOTREACHED*/
    }
  }

  FocusSimulator sim(sim_args);
  sim.Print();

  RunningFocus running_focus("/tmp/runningfocus.log");

  for (int t=0; t<14000; t += 60) {
    JULIAN raw_time(2459000.0 + (t/(24.0*3600.0)));
    sim_time = t;

    double blur = sim.GetImageBlur(t, sim_focuser_ticks);
    running_focus.AddPoint(blur, sim_focuser_ticks, raw_time);
    running_focus.UpdateFocus();
  }
#endif
}
#endif
