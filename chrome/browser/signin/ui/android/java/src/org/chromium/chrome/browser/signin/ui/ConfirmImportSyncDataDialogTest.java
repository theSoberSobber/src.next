// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.IBinder;
import android.view.WindowManager;

import androidx.test.espresso.Root;
import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.DummyUiActivity;
import org.chromium.ui.test.util.ThemedDummyUiActivityTestRule;

/**
 * Instrumentation tests for {@link ConfirmImportSyncDataDialogCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ConfirmImportSyncDataDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    private static class ToastMatcher extends TypeSafeMatcher<Root> {
        @Override
        public void describeTo(Description description) {
            description.appendText("For Managed Domain");
        }

        @Override
        public boolean matchesSafely(Root root) {
            int type = root.getWindowLayoutParams().get().type;
            if ((type == WindowManager.LayoutParams.TYPE_TOAST)) {
                IBinder windowToken = root.getDecorView().getWindowToken();
                IBinder appToken = root.getDecorView().getApplicationWindowToken();
                if (windowToken == appToken) {
                    return true;
                }
            }
            return false;
        }
    }

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    // Disable animations to reduce flakiness.
    @ClassRule
    public static final DisableAnimationsTestRule sNoAnimationsRule =
            new DisableAnimationsTestRule();

    @ClassRule
    public static final ThemedDummyUiActivityTestRule<DummyUiActivity> sActivityTestRule =
            new ThemedDummyUiActivityTestRule<>(
                    DummyUiActivity.class, R.style.ColorOverlay_ChromiumAndroid);

    @Mock
    private ConfirmImportSyncDataDialogCoordinator.Listener mListenerMock;

    @Mock
    private SigninManager mSigninManagerMock;

    private ModalDialogManager mDialogManager;
    private ConfirmImportSyncDataDialogCoordinator mDialogCoordinator;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        Profile.setLastUsedProfileForTesting(mock(Profile.class));
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        mDialogManager = new ModalDialogManager(
                new AppModalPresenter(sActivityTestRule.getActivity()), ModalDialogType.APP);
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showConfirmImportSyncDataDialog();
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(true);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManaged() {
        showConfirmImportSyncDataDialog();
        onView(withId(R.id.sync_confirm_import_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(false);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testNegativeButton() {
        showConfirmImportSyncDataDialog();
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());
        verify(mListenerMock, never()).onConfirm(anyBoolean());
        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testListenerOnCancelNotCalledWhenDialogDismissedInternally() {
        showConfirmImportSyncDataDialog();
        mDialogCoordinator.dismissDialog();
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testListenerOnCancelCalledWhenDialogDismissedByUser() {
        showConfirmImportSyncDataDialog();
        onView(isRoot()).perform(pressBack());
        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testToastOfConfirmImportOptionForManagedAccount() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showConfirmImportSyncDataDialog();
        onView(withId(R.id.sync_confirm_import_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.managed_by_your_organization))
                .inRoot(new ToastMatcher())
                .check(matches(isDisplayed()));
    }

    private void showConfirmImportSyncDataDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogCoordinator = new ConfirmImportSyncDataDialogCoordinator(
                    sActivityTestRule.getActivity(), mDialogManager, mListenerMock,
                    "old.testaccount@gmail.com", "new.testaccount@gmail.com");
        });
    }
}
