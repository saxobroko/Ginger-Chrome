# Feature usage metrics

The Feature Usage Metrics component provides a unified approach to report
feature usage events. The tracked events have standard names and new features
could be easily incorporated on the data analytics side.

[TOC]

## Objectives

This component is the part of Standard Feature Usage Logging (Googlers could see
go/sful). The goal is to make metrics calculation and analysis easily scalable
to the new features. Both for feature owners and for data analytics team.

## Overview

The following events are reported by the component (for details see
[FeatureUsageEvent][1])
* Is device eligible for the feature?
* Has the user enabled the feature for the device?
* Successful attempt to use the feature.
* Failed attempt to use the feature.
* Record the usage time of the feature.

The first two are reported periodically every 30 minutes. To correctly track 1-,
7-, 28-days users. The feature usage component encapsulates this logic.

For more details see original [CL](https://crrev.com/c/2596263)

[1]: https://source.chromium.org/search?q=FeatureUsageEvent%20f:metrics&ss=chromium

## Integrate your new feature with the component

You need to do the following things to integrate your feature, all described in
detail below.

*   [Append your feature to the usage logging features list](#Appending-your-feature)
*   [Create a component object](#Creating-component-object) and pass the
    delegate inside.
*   [Record feature usage](#Recording-feature-usage)

### Appending your feature

You need to add a new variant into `<variants
name="FeaturesLoggingUsageEvents">`

*   `//tools/metrics/histograms/histograms_xml/chromeos/histograms.xml`:

```
  <variant name="YourFeature" summary="your feature">
    <owner>you@chromium.org<owner>
    <owner>your-team@chromium.org</owner>
  </variant>
```

### Creating component object
You need to implement `FeatureUsageMetrics::Delegate` and pass it to the
`FeatureUsageMetrics`. Delegate is called to report periodic events (eligible,
enabled).

```c++
class MyDelegate : public FeatureUsageMetrics::Delegate {
 public:
  bool IsEligible() const final {
    ...
  }
  // If `IsEnabled` returns true `IsEligible` must return true too.
  bool IsEnabled() const final {
    ...
  }
};
```

```c++
feature_usage_metrics_ = std::make_unique<FeatureUsageMetrics>(
        "YourFeature", my_delegate);
```

`YourFeature` must correspond to the histogram and never change. `MyDelegate`
object must outlive the `FeatureUsageMetrics` object.

### Recording feature usage
Call `feature_usage_metrics_->RecordUsage(bool success);` on every usage
attempt. Success indicates whether or not the attempt to use was successful.
Your feature might not have failed attempts. In that case always call with
`success=true`.

`MyDelegate::IsEligible` and `MyDelegate::IsEnabled` functions must return
`true` when `RecordUsage` is called.

#### Recording usage time
If your feature has a notion of time usage use
`feature_usage_metrics_->StartUsage();` and
`feature_usage_metrics_->StopUsage();` to record feature usage time.

* There should be no consecutive `StartUsage` calls without `StopUsage` call
in-between.
* After `StartUsage` is called the usage time is reported periodically together
with `IsEligible` and `IsEnabled`.
* If `StartUsage` is not followed by `StopUsage` the remaining usage time is
recorded at the object shutdown.
* `StartUsage` must be preceded by exactly one `RecordUsage(true)`. There should
be no `RecordUsage` calls in-between `StartUsage` and `StopUsage` calls.

Example:
```c++
// feature_usage_metrics_->StartUsage(); should be preceded by RecordUsage(true)
feature_usage_metrics_->RecordUsage(false);
// feature_usage_metrics_->StartUsage(); should be preceded by RecordUsage(true)
feature_usage_metrics_->RecordUsage(true);
feature_usage_metrics_->StartUsage();
feature_usage_metrics_->StopUsage();
// feature_usage_metrics_->StartUsage(); should be preceded by RecordUsage(true)
feature_usage_metrics_->RecordUsage(true);
feature_usage_metrics_->RecordUsage(true);
// feature_usage_metrics_->StartUsage(); should be preceded by exactly one RecordUsage(true)
....
feature_usage_metrics_->StartUsage();
feature_usage_metrics_->reset(); // Usage time is recorded similar to StopUsage
```

### Testing
Use `base::HistogramTester` to verify attempt events are reported.

```c++
base::HistogramTester histogram_tester;
// Emulate failed usage attempt.
histogram_tester.ExpectBucketCount(
    "ChromeOS.FeatureUsage.YouFeature",
    static_cast<int>(FeatureUsageMetrics::Event::kUsedWithFailure), 1);
  ...
// Emulate successful usage attempt.
histogram_tester.ExpectBucketCount(
    "ChromeOS.FeatureUsage.YouFeature",
    static_cast<int>(FeatureUsageMetrics::Event::kUsedWithSuccess), 1);
  ...
// Emulate the amount of time |usetime| (base::TimeDelta) using the feature.
histogram_tester_->ExpectTimeBucketCount(
  "ChromeOS.FeatureUsage.YouFeature.Usetime", usetime, 1);
```

