import Cocoa
import AppCenter
import AppCenterCrashes

class AppCenterCrash: NSObject, CrashesDelegate {
    static let secret = Bundle.main.object(forInfoDictionaryKey: "AppCenterSecret") as! String

    override init() {
        super.init()
        // Enable catching uncaught exceptions thrown on the main thread
        UserDefaults.standard.register(defaults: ["NSApplicationCrashOnExceptions": true])
//        AppCenter.logLevel = .verbose
        // without this, appcenter makes network call just from AppCenter.start; we only want networking when sending reports
        AppCenter.networkRequestsAllowed = false
        AppCenter.start(withAppSecret: AppCenterCrash.secret, services: [Crashes.self])
        Crashes.delegate = self
        Crashes.userConfirmationHandler = confirmationHandler
    }

    /// at launch, the crash report handler can be called before some things are not yet ready; we ensure they are
    func initNecessaryFacilities() {
        if BackgroundWork.crashReportsQueue == nil {
            BackgroundWork.crashReportsQueue = DispatchQueue.globalConcurrent("crashReportsQueue", .utility)
        }
    }

    func confirmationHandler(_ errorReports: [ErrorReport]) -> Bool {
        initNecessaryFacilities()
        let shouldSend = checkIfShouldSend()
        BackgroundWork.crashReportsQueue.async {
            AppCenter.networkRequestsAllowed = shouldSend
            Crashes.notify(with: shouldSend ? .send : .dontSend)
            AppCenter.networkRequestsAllowed = false
        }
        return true
    }

    func checkIfShouldSend() -> Bool {
        App.app.activate(ignoringOtherApps: true)
        let alert = NSAlert()
        alert.alertStyle = .warning
        alert.messageText = NSLocalizedString("Send a crash report?", comment: "")
        alert.informativeText = String(format: NSLocalizedString("%@ crashed last time you used it. Sending a crash report will help get the issue fixed", comment: "%@ is FatDriveSorter"), App.name)
        alert.addButton(withTitle: NSLocalizedString("Send", comment: "")).setAccessibilityFocused(true)
        let cancelButton = alert.addButton(withTitle: NSLocalizedString("Don’t send", comment: ""))
        cancelButton.keyEquivalent = "\u{1b}"
        let checkbox = NSButton(checkboxWithTitle: NSLocalizedString("Remember my choice", comment: ""), target: nil, action: nil)
        alert.accessoryView = checkbox
        let userChoice = alert.runModal()
        return userChoice == .alertFirstButtonReturn
    }

    func attachments(with crashes: Crashes, for errorReport: ErrorReport) -> [ErrorAttachmentLog]? {
        return [ErrorAttachmentLog.attachment(withText: DebugProfile.make(), filename: "debug-profile.md")!]
    }
}
