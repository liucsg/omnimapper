#pragma once

#include <gtsam/nonlinear/NonlinearFactorGraph.h>
#include <gtsam/nonlinear/Values.h>

namespace omnimapper {
class OutputPlugin {
 public:
  virtual void Update(
      boost::shared_ptr<gtsam::Values>& vis_values,
      boost::shared_ptr<gtsam::NonlinearFactorGraph>& vis_graph) = 0;
};

}  // namespace omnimapper
