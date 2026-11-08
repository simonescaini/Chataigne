# Haptic to Move correction

Before: isolated baseline passed 59 range checks and 162 native core checks, including the prior rejection policy. After: see handoff-range and handoff-core logs. Only rejection expectations are replaced; Link-master and model/range assertions remain.

Service validates the authored Event and curve, Runtime claims exact owners, Driver packs the existing model-specific packets. A Move on either member supersedes the whole Haptic pair and invalidates its generations. The other member is released, not described as still coupled. The Move uses existing stop, CSP mode 5, current/speed limits, enable and target setup. This is not simultaneous Haptic/Move composition.

RS02 manual https://files.seeedstudio.com/products/RobStride/Product%20Literature/RS02/RS02User%20Manual251112.pdf, page 1, requires stopping before mode changes. That supports the existing ordering, not RS01 firmware equivalence or new combined-control modes. Actual installed firmware and physical transition behaviour remain NOT RUN.

The direct fake Runtime driver tests validate packets, pair scope, stale torque exclusion and unrelated ownership. Application Action/Mapping and cue-path tests are recorded separately. Service launch now reports a runtime launch failure instead of returning as though it succeeded. Transport enqueue and transmission still do not prove hardware acceptance.
