// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;
import android.view.Display;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroidManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import org.chromium.base.ContextUtils;

/**
 * Collection of utility methods that operates on Tab.
 */
public class TabUtils {
    private static final String REQUEST_DESKTOP_SCREEN_WIDTH_PARAM = "screen_width_dp";

    /**
     * Define the callers of NavigationControllerImpl#setUseDesktopUserAgent.
     */
    @IntDef({UseDesktopUserAgentCaller.ON_MENU_OR_KEYBOARD_ACTION,
            UseDesktopUserAgentCaller.LOAD_IF_NEEDED, UseDesktopUserAgentCaller.RELOAD,
            UseDesktopUserAgentCaller.RELOAD_IGNORING_CACHE, UseDesktopUserAgentCaller.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UseDesktopUserAgentCaller {
        int ON_MENU_OR_KEYBOARD_ACTION = 0;
        int LOAD_IF_NEEDED = 1;
        int RELOAD = 2;
        int RELOAD_IGNORING_CACHE = 3;
        int OTHER = 4;
    }

    // Do not instantiate this class.
    private TabUtils() {}

    /**
     * @return {@link Activity} associated with the given tab.
     */
    @Nullable
    public static Activity getActivity(Tab tab) {
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        if (webContents == null || webContents.isDestroyed()) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        return window != null ? window.getActivity().get() : null;
    }

    /**
     * Provides an estimate of the contents size.
     *
     * The estimate is likely to be incorrect. This is not a problem, as the aim
     * is to avoid getting a different layout and resources than needed at
     * render time.
     * @param context The application context.
     * @return The estimated prerender size in pixels.
     */
    public static Rect estimateContentSize(Context context) {
        // The size is estimated as:
        // X = screenSizeX
        // Y = screenSizeY - top bar - bottom bar - custom tabs bar
        // The bounds rectangle includes the bottom bar and the custom tabs bar as well.
        Rect screenBounds = new Rect();
        Point screenSize = new Point();
        Display display = DisplayAndroidManager.getDefaultDisplayForContext(context);
        display.getSize(screenSize);
        Resources resources = context.getResources();
        int statusBarId = resources.getIdentifier("status_bar_height", "dimen", "android");
        try {
            screenSize.y -= resources.getDimensionPixelSize(statusBarId);
        } catch (Resources.NotFoundException e) {
            // Nothing, this is just a best effort estimate.
        }
        screenBounds.set(0,
                resources.getDimensionPixelSize(R.dimen.custom_tabs_control_container_height),
                screenSize.x, screenSize.y);
        return screenBounds;
    }

    public static Tab fromWebContents(WebContents webContents) {
        return TabImplJni.get().fromWebContents(webContents);
    }

    /**
     * Call when tab need to switch user agent between desktop and mobile.
     * @param tab The tab to be switched the user agent.
     * @param switchToDesktop Whether switching the user agent to desktop.
     * @param forcedByUser Whether this was triggered by users action.
     * @param caller The caller of this method.
     */
    public static void switchUserAgent(Tab tab, boolean switchToDesktop, boolean forcedByUser,
            @UseDesktopUserAgentCaller int caller) {
        final boolean reloadOnChange = !tab.isNativePage();
        tab.getWebContents().getNavigationController().setUseDesktopUserAgent(
                switchToDesktop, reloadOnChange);
        if (forcedByUser) ((TabImpl) tab).setUserForcedUserAgent();
    }

    /**
     * Get UseDesktopUserAgent setting from webContents.
     * @param webContents The webContents used to retrieve UseDesktopUserAgent setting.
     * @return Whether the webContents is set to use desktop user agent.
     */
    public static boolean isUsingDesktopUserAgent(WebContents webContents) {
        return webContents != null
                && webContents.getNavigationController().getUseDesktopUserAgent();
    }

    /**
     * Get tabUserAgent from the tab, which represents the tab level RDS setting.
     * @param tab The tab used to retrieve tabUserAgent.
     * @return The tab level RDS setting.
     */
    public static @TabUserAgent int getTabUserAgent(Tab tab) {
        @TabUserAgent
        int tabUserAgent = CriticalPersistedTabData.from(tab).getUserAgent();
        WebContents webContents = tab.getWebContents();
        boolean currentRequestDesktopSite = isUsingDesktopUserAgent(webContents);
        // TabUserAgent.UNSET means this is a pre-existing tab from an earlier build. In this case
        // we set the TabUserAgent bit based on last committed entry's user agent. If webContents is
        // null, this method is triggered too early, and we cannot read the last committed entry's
        // user agent yet. We will skip for now and let the following call set the TabUserAgent bit.
        if (webContents != null && tabUserAgent == TabUserAgent.UNSET) {
            if (currentRequestDesktopSite) {
                tabUserAgent = TabUserAgent.DESKTOP;
            } else {
                tabUserAgent = TabUserAgent.DEFAULT;
                if (ContextUtils.getAppSharedPreferences().getBoolean("desktop_mode", false))
                     tabUserAgent = TabUserAgent.DESKTOP;
            }
            CriticalPersistedTabData.from(tab).setUserAgent(tabUserAgent);
        }
        return tabUserAgent;
    }

    /**
     * Read Request Desktop Site ContentSettings.
     * @param profile The profile used to retrieve ContentSettings.
     * @param webContents The webContents used to retrieve Url for site level setting.
     * @return Whether Request Desktop Site is enabled in ContentSettings.
     */
    public static boolean readRequestDesktopSiteContentSettings(
            Profile profile, WebContents webContents) {
        if (ContentFeatureList.isEnabled(ContentFeatureList.REQUEST_DESKTOP_SITE_EXCEPTIONS)) {
            return webContents != null
                    && TabUtils.isDesktopSiteEnabled(profile, webContents.getVisibleUrl());
        } else {
            return TabUtils.isDesktopSiteGlobalEnabled(profile);
        }
    }

    /**
     * Check if the tab is large enough for displaying desktop sites. This method will only check
     * for tablets, if the device is a phone, will return false regardless of tab size.
     * @param tab The tab to be checked if the size is large enough for desktop site.
     * @return Whether or not the screen size is large enough for desktop sites.
     */
    public static boolean isTabLargeEnoughForDesktopSite(Tab tab) {
        if ((ContextUtils.getAppSharedPreferences().getBoolean("desktop_mode", false)))
            return true;
        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(tab.getContext())) {
            // The device is a phone, do not check the tab size.
            return false;
        }
        Activity activity = ((TabImpl) tab).getActivity();
        if (activity == null) {
            // It is possible that we are in custom tabs or tests, and need to access the activity
            // differently.
            activity = ApplicationStatus.getLastTrackedFocusedActivity();
            if (activity == null) return false;
        }
        int windowWidth = activity.getWindow().getDecorView().getWidth();
        int minWidthForDesktopSite = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.REQUEST_DESKTOP_SITE_FOR_TABLETS,
                REQUEST_DESKTOP_SCREEN_WIDTH_PARAM,
                /* Set a very large size as default to serve as a disabled screen width. */ 4096);

        return minWidthForDesktopSite <= windowWidth;
    }
}
