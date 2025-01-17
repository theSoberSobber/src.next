// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.v2.FeedProcessScopeDependencyProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * Provides activity, darkmode and logging context for a single surface.
 */
@JNINamespace("feed::android")
class FeedSurfaceScopeDependencyProvider implements SurfaceScopeDependencyProvider {
    private static final String TAG = "Feed";
    private final Activity mActivity;
    private final Context mActivityContext;
    private final boolean mDarkMode;
    private final LoggingEnabledDelegate mLoggingEnabledDelegate;

    public interface LoggingEnabledDelegate {
        boolean isLoggingEnabledForCurrentStream();
    }

    public FeedSurfaceScopeDependencyProvider(Activity activity, Context activityContext,
            boolean darkMode, LoggingEnabledDelegate loggingEnabledDelegate) {
        mActivityContext = FeedProcessScopeDependencyProvider.createFeedContext(activityContext);
        mDarkMode = darkMode;
        mActivity = activity;
        mLoggingEnabledDelegate = loggingEnabledDelegate;
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public Context getActivityContext() {
        return mActivityContext;
    }

    @Override
    public boolean isDarkModeEnabled() {
        return mDarkMode;
    }

    @Override
    public AutoplayPreference getAutoplayPreference() {
        assert ThreadUtils.runningOnUiThread();
        @VideoPreviewsType
        int videoPreviewsType = FeedServiceBridge.getVideoPreviewsTypePreference();
        switch (videoPreviewsType) {
            case VideoPreviewsType.NEVER:
                return AutoplayPreference.AUTOPLAY_DISABLED;
            case VideoPreviewsType.WIFI_AND_MOBILE_DATA:
                return AutoplayPreference.AUTOPLAY_ON_WIFI_AND_MOBILE_DATA;
            case VideoPreviewsType.WIFI:
            default:
                return AutoplayPreference.AUTOPLAY_ON_WIFI_ONLY;
        }
    }

    /**
     * FeedLoggingDependencyProvider implementation.
     */

    @Override
    public String getAccountName() {
        // Don't return account name if there's a signed-out session ID.
        if (!getSignedOutSessionId().isEmpty()) {
            return "";
        }
        assert ThreadUtils.runningOnUiThread();
        CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return (primaryAccount == null) ? "" : primaryAccount.getEmail();
    }

    @Override
    public String getClientInstanceId() {
        // Don't return client instance id if there's a signed-out session ID.
        if (!getSignedOutSessionId().isEmpty()) {
            return "";
        }
        assert ThreadUtils.runningOnUiThread();
        return FeedServiceBridge.getClientInstanceId();
    }

    @Override
    public int[] getExperimentIds() {
        assert ThreadUtils.runningOnUiThread();
        return FeedSurfaceScopeDependencyProviderJni.get().getExperimentIds();
    }

    @Override
    public boolean isActivityLoggingEnabled() {
        assert ThreadUtils.runningOnUiThread();
        return mLoggingEnabledDelegate.isLoggingEnabledForCurrentStream();
    }

    @Override
    public String getSignedOutSessionId() {
        ThreadUtils.runningOnUiThread();
        return FeedSurfaceScopeDependencyProviderJni.get().getSessionId();
    }

    /**
     * Stores a view FeedAction for eventual upload. 'data' is a serialized FeedAction protobuf
     * message.
     */
    @Override
    public void processViewAction(byte[] data) {
        FeedSurfaceScopeDependencyProviderJni.get().processViewAction(data);
    }

    @Override
    public void reportOnUploadVisibilityLog(boolean success) {
        RecordHistogram.recordBooleanHistogram(
                "ContentSuggestions.Feed.UploadVisibilityLog", success);
    }

    @Override
    public void reportVideoPlayEvent(boolean isMutedAutoplay, @VideoPlayEvent int event) {
        Log.i(TAG, "Feed video event %d", event);
        RecordHistogram.recordEnumeratedHistogram(
                getVideoHistogramName(isMutedAutoplay, "PlayEvent"), event,
                VideoPlayEvent.NUM_ENTRIES);
    }

    @Override
    public void reportVideoInitializationError(
            boolean isMutedAutoplay, @VideoInitializationError int error) {
        Log.i(TAG, "Feed video initialization error %d", error);
        RecordHistogram.recordEnumeratedHistogram(
                getVideoHistogramName(isMutedAutoplay, "InitializationError"), error,
                VideoInitializationError.NUM_ENTRIES);
    }

    @Override
    public void reportVideoPlayError(boolean isMutedAutoplay, @VideoPlayError int error) {
        Log.i(TAG, "Feed video play error %d", error);
        RecordHistogram.recordEnumeratedHistogram(
                getVideoHistogramName(isMutedAutoplay, "PlayError"), error,
                VideoPlayError.NUM_ENTRIES);
    }

    @Override
    public Rect getToolbarGlobalVisibleRect() {
        Rect bounds = new Rect();
        View toolbarView = mActivity.findViewById(R.id.toolbar);
        if (toolbarView == null) {
            return bounds;
        }
        toolbarView.getGlobalVisibleRect(bounds);
        return bounds;
    }

    private static String getVideoHistogramName(boolean isMutedAutoplay, String partName) {
        String name = "ContentSuggestions.Feed.";
        name += (isMutedAutoplay ? "AutoplayMutedVideo." : "NormalUnmutedVideo.");
        name += partName;
        return name;
    }

    @NativeMethods
    interface Natives {
        int[] getExperimentIds();
        String getSessionId();
        void processViewAction(byte[] data);
    }
}
