// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "components/permissions/permission_request.h"
#include "url/origin.h"

// A permission request that presents the user with a choice to allow or deny
// multiple downloads from the same site. This confirmation step protects
// against "carpet-bombing", where a malicious site forces multiple downloads on
// an unsuspecting user.
class DownloadPermissionRequest : public permissions::PermissionRequest {
 public:
  DownloadPermissionRequest(
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host,
      const url::Origin& request_origin);
  ~DownloadPermissionRequest() override;

 private:
  // permissions::PermissionRequest:
  permissions::RequestType GetRequestType() const override;
#if defined(OS_ANDROID)
  std::u16string GetMessageText() const override;
#else
  std::u16string GetMessageTextFragment() const override;
#endif
  GURL GetOrigin() const override;
  void PermissionGranted(bool is_one_time) override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;

  base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host_;
  url::Origin request_origin_;

  DISALLOW_COPY_AND_ASSIGN(DownloadPermissionRequest);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_
