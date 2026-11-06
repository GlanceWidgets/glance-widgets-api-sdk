#include "glance_widgets/signature.hpp"
#include <cassert>
using namespace glance_widgets;
int main() {
  assert(sign("1700000000000", "this is secret") ==
         "sSFWELbV2YwjdDQhWZwTcWlX5BWUx5J6TPpsZmuPii0=");
  assert(sign(true, "s") == sign("true", "s"));
  assert(sign(nullptr, "s") == sign("null", "s"));
  assert(sign(-0.0, "s") == sign("0", "s"));
  bool rejected = false;
  try { sign(1, true); } catch (const GlanceError&) { rejected = true; }
  assert(rejected);
  SignedRequest request{"1700000000000", sign("1700000000000", "s")};
  VerifyOptions options{1700000000000.0, std::nullopt};
  assert(verify_signature(request, "s", options));
  options.now = 1700000400000.0;
  assert(!verify_signature(request, "s", options));
  request.timestamp = "+1700000000000";
  assert(!verify_signature(request, "s", options));
}
