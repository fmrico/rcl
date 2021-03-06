// Copyright 2016 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>
#include <string>
#include <thread>

#include "rcutils/logging_macros.h"

#include "rcl/service.h"

#include "rcl/rcl.h"

#include "test_msgs/srv/primitives.h"

#include "osrf_testing_tools_cpp/scope_exit.hpp"
#include "rcl/error_handling.h"

bool
wait_for_service_to_be_ready(
  rcl_service_t * service,
  size_t max_tries,
  int64_t period_ms)
{
  rcl_wait_set_t wait_set = rcl_get_zero_initialized_wait_set();
  rcl_ret_t ret = rcl_wait_set_init(&wait_set, 0, 0, 0, 0, 1, rcl_get_default_allocator());
  if (ret != RCL_RET_OK) {
    RCUTILS_LOG_ERROR_NAMED(
      ROS_PACKAGE_NAME, "Error in wait set init: %s", rcl_get_error_string().str);
    return false;
  }
  OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT({
    if (rcl_wait_set_fini(&wait_set) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in wait set fini: %s", rcl_get_error_string().str);
      throw std::runtime_error("error waiting for service to be ready");
    }
  });
  size_t iteration = 0;
  do {
    ++iteration;
    if (rcl_wait_set_clear(&wait_set) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in wait_set_clear: %s", rcl_get_error_string().str);
      return false;
    }
    if (rcl_wait_set_add_service(&wait_set, service, NULL) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in wait_set_add_service: %s", rcl_get_error_string().str);
      return false;
    }
    ret = rcl_wait(&wait_set, RCL_MS_TO_NS(period_ms));
    if (ret == RCL_RET_TIMEOUT) {
      continue;
    }
    if (ret != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(ROS_PACKAGE_NAME, "Error in wait: %s", rcl_get_error_string().str);
      return false;
    }
    for (size_t i = 0; i < wait_set.size_of_services; ++i) {
      if (wait_set.services[i] && wait_set.services[i] == service) {
        return true;
      }
    }
  } while (iteration < max_tries);
  return false;
}

int main(int argc, char ** argv)
{
  int main_ret = 0;
  {
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    rcl_ret_t ret = rcl_init_options_init(&init_options, rcl_get_default_allocator());
    if (RCL_RET_OK != ret) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in rcl init options init: %s", rcl_get_error_string().str);
      return -1;
    }
    rcl_context_t context = rcl_get_zero_initialized_context();
    if (rcl_init(argc, argv, &init_options, &context) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in rcl init: %s", rcl_get_error_string().str);
      return -1;
    }
    OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT({
      if (rcl_shutdown(&context) != RCL_RET_OK) {
        RCUTILS_LOG_ERROR_NAMED(
          ROS_PACKAGE_NAME, "Error shutting down rcl: %s", rcl_get_error_string().str);
        main_ret = -1;
      }
      if (rcl_context_fini(&context) != RCL_RET_OK) {
        RCUTILS_LOG_ERROR_NAMED(
          ROS_PACKAGE_NAME, "Error finalizing rcl context: %s", rcl_get_error_string().str);
        main_ret = -1;
      }
    });
    ret = rcl_init_options_fini(&init_options);
    rcl_node_t node = rcl_get_zero_initialized_node();
    const char * name = "service_fixture_node";
    rcl_node_options_t node_options = rcl_node_get_default_options();
    if (rcl_node_init(&node, name, "", &context, &node_options) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in node init: %s", rcl_get_error_string().str);
      return -1;
    }
    OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT({
      if (rcl_node_fini(&node) != RCL_RET_OK) {
        RCUTILS_LOG_ERROR_NAMED(
          ROS_PACKAGE_NAME, "Error in node fini: %s", rcl_get_error_string().str);
        main_ret = -1;
      }
    });

    const rosidl_service_type_support_t * ts = ROSIDL_GET_SRV_TYPE_SUPPORT(
      test_msgs, srv, Primitives);
    const char * service_name = "primitives";

    rcl_service_t service = rcl_get_zero_initialized_service();
    rcl_service_options_t service_options = rcl_service_get_default_options();
    ret = rcl_service_init(&service, &node, ts, service_name, &service_options);
    if (ret != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in service init: %s", rcl_get_error_string().str);
      return -1;
    }

    OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT({
      if (rcl_service_fini(&service, &node)) {
        RCUTILS_LOG_ERROR_NAMED(
          ROS_PACKAGE_NAME, "Error in service fini: %s", rcl_get_error_string().str);
        main_ret = -1;
      }
    });

    // Initialize a response.
    test_msgs__srv__Primitives_Response service_response;
    // TODO(dirk-thomas) zero initialization necessary until
    // https://github.com/ros2/ros2/issues/397 is implemented
    memset(&service_response, 0, sizeof(test_msgs__srv__Primitives_Response));
    test_msgs__srv__Primitives_Response__init(&service_response);
    OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT({
      test_msgs__srv__Primitives_Response__fini(&service_response);
    });

    // Block until a client request comes in.

    if (!wait_for_service_to_be_ready(&service, 1000, 100)) {
      RCUTILS_LOG_ERROR_NAMED(ROS_PACKAGE_NAME, "Service never became ready");
      return -1;
    }

    // Take the pending request.
    test_msgs__srv__Primitives_Request service_request;
    // TODO(dirk-thomas) zero initialization necessary until
    // https://github.com/ros2/ros2/issues/397 is implemented
    memset(&service_request, 0, sizeof(test_msgs__srv__Primitives_Request));
    test_msgs__srv__Primitives_Request__init(&service_request);
    OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT({
      test_msgs__srv__Primitives_Request__fini(&service_request);
    });
    rmw_request_id_t header;
    // TODO(jacquelinekay) May have to check for timeout error codes
    if (rcl_take_request(&service, &header, &service_request) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in take_request: %s", rcl_get_error_string().str);
      return -1;
    }

    // Sum the request and send the response.
    service_response.uint64_value = service_request.uint8_value + service_request.uint32_value;
    if (rcl_send_response(&service, &header, &service_response) != RCL_RET_OK) {
      RCUTILS_LOG_ERROR_NAMED(
        ROS_PACKAGE_NAME, "Error in send_response: %s", rcl_get_error_string().str);
      return -1;
    }
    // Our scope exits should take care of fini for everything
    // stick around until launch gives us a signal to exit
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  return main_ret;
}
