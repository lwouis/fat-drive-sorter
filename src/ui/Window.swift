class Window: NSWindow {
    static var window: Window!
    var daSession: DASession!
    var drives = [Drive]()
    var drive: Drive? { get { drives.first { $0.name == driveDropdown.title } } }
    var filesOnDrive = File("/")
    let defaultDirectoryTitle = "Select a directory"
    var filesView: FilesView!
    var clearButton: NSButton!
    var driveDropdown: NSPopUpButton!
    var noDriveText: NSTextField!
    var directoryButton: NSButton!
    var sortByDropdown: NSPopUpButton!
    var listButton: NSButton!
    var sortButton: NSButton!

    convenience init() {
        self.init(contentRect: .init(x: 0, y: 0, width: 600, height: 400), styleMask: [.titled, .miniaturizable, .closable], backing: .buffered, defer: false)
        Window.window = self
        daSession = DASessionCreate(kCFAllocatorDefault)!
        DASessionScheduleWithRunLoop(daSession, CFRunLoopGetMain(), CFRunLoopMode.defaultMode.rawValue)
        setupWindow()
        observeVolumeChanges()
        updateDrives()
    }

    private func updateDrives() {
        let keys: Set<URLResourceKey> = [.volumeLocalizedNameKey, .volumeIsRemovableKey, .volumeIsEjectableKey]
        drives = getDrives(keys)
        directoryButton.isEnabled = !drives.isEmpty
        listButton.isEnabled = !drives.isEmpty
        sortButton.isEnabled = !drives.isEmpty
        noDriveText.isHidden = !drives.isEmpty
        let currentItems = (0..<driveDropdown.numberOfItems).map { driveDropdown.item(at: $0)!.title }
        let newItems = drives.map { $0.name }
        currentItems.forEach { currentItem in
            if !newItems.contains(currentItem) {
                driveDropdown.removeItem(withTitle: currentItem)
            }
        }
        newItems.forEach { newItem in
            if !currentItems.contains(newItem) {
                driveDropdown.addItem(withTitle: newItem)
            }
        }
        if #available(macOS 11.0, *) {
            (0..<driveDropdown.numberOfItems).forEach {
                driveDropdown.item(at: $0)?.image = NSImage(systemSymbolName: "externaldrive", accessibilityDescription: nil)!
            }
        }
    }

    private func getDrives(_ keys: Set<URLResourceKey>) -> [Drive] {
        return FileManager.default
                .mountedVolumeURLs(includingResourceValuesForKeys: Array(keys), options: [.skipHiddenVolumes])!
                .compactMap {
                    let values = try? $0.resourceValues(forKeys: keys)
                    let name = values?.volumeLocalizedName
                    let isRemovable = values?.volumeIsRemovable ?? false
                    let isEjectable = values?.volumeIsEjectable ?? false
                    if isRemovable && isEjectable, let name = name,
                       let daDisk = DADiskCreateFromVolumePath(kCFAllocatorDefault, daSession, $0 as CFURL),
                       let bsdName = DADiskGetBSDName(daDisk) {
                        let bsdNode = "/dev/" + String(cString: bsdName)
                        return Drive($0.path, name, bsdNode, daDisk)
                    }
                    return nil
                }
    }

    private func observeVolumeChanges() {
        [NSWorkspace.didMountNotification, NSWorkspace.didUnmountNotification].forEach {
            NSWorkspace.shared.notificationCenter.addObserver(forName: $0, object: nil, queue: nil) { notification in
                self.updateDrives()
            }
        }
    }

    private func setupWindow() {
        title = App.name
        center()
        makeKeyAndOrderFront(nil)
        driveDropdown = NSPopUpButton(frame: .zero)
        driveDropdown.addItems(withTitles: drives.map { $0.name })
        sortByDropdown = NSPopUpButton(frame: .zero)
        sortByDropdown.addItems(withTitles: Order.allCases.map { $0.localizedString })
        if #available(macOS 11.0, *) {
            for i in 0..<sortByDropdown.numberOfItems {
                let item = sortByDropdown.item(at: i)!
                item.image = Order.fromLocalizedString(item.title).image()
            }
        }
        directoryButton = NSButton(title: defaultDirectoryTitle, target: self, action: #selector(selectDirectory))
        listButton = NSButton(title: NSLocalizedString("See current file order", comment: ""), target: self, action: #selector(list))
        sortButton = NSButton(title: NSLocalizedString("Sort", comment: ""), target: self, action: #selector(sort))
        let buttons = NSStackView(views: [listButton, sortButton])
        clearButton = NSButton(image: NSImage(named: NSImage.stopProgressFreestandingTemplateName)!, target: self, action: #selector(clearDirectory))
        clearButton.isBordered = false
        clearButton.isHidden = true
        let directoryAndClear = NSStackView(views: [directoryButton, clearButton])
        filesView = FilesView.generate(filesOnDrive)
        let scrollView = NSScrollView()
        scrollView.hasVerticalScroller = true
        scrollView.hasHorizontalScroller = true
        scrollView.scrollerStyle = .overlay
        scrollView.documentView = filesView
        noDriveText = NSTextField(labelWithString: NSLocalizedString("No drive detected", comment: ""))
        noDriveText.textColor = .red
        noDriveText.isHidden = true
        let driveDropdownStackView = NSStackView(views: [driveDropdown, noDriveText])
        let subGridView = NSGridView(views: [
            [NSTextField(labelWithString: NSLocalizedString("Drive:", comment: "")), driveDropdownStackView],
            [NSTextField(labelWithString: NSLocalizedString("Directory (optional):", comment: "")), directoryAndClear],
            [NSTextField(labelWithString: NSLocalizedString("Order:", comment: "")), sortByDropdown],
        ])
        subGridView.column(at: 0).xPlacement = .trailing
        subGridView.rowSpacing = 10
        subGridView.columnSpacing = 10
        let gridView = NSGridView(views: [
            [subGridView],
            [buttons],
            [scrollView],
        ])
        gridView.xPlacement = .center
        gridView.rowSpacing = 20
        gridView.row(at: 0).topPadding = 20
        gridView.row(at: gridView.numberOfRows - 1).bottomPadding = 20
        gridView.column(at: 0).leadingPadding = 20
        gridView.column(at: gridView.numberOfColumns - 1).trailingPadding = 20
        contentView = gridView
        setContentSize(.init(width: 600, height: 600))
    }

    static let afterUnmount: DADiskUnmountCallback = { daDisk, dissenter, context in
        debugPrint("afterUnmount")
        if let drive = window.drive {
            let url = Bundle.main.url(forAuxiliaryExecutable: "fatsort")!
            if window.listButton.isEnabled {
                let command = "\(url.path) \(drive.bsdNode)\(Window.fatsortFlagsFromUiSettings())"
                do {
                    let data = try Authorization.executeWithPrivileges(command)
                    let stdout = String(bytes: data.readDataToEndOfFile(), encoding: .utf8)!
                    debugPrint("fatsort", command, stdout)
                } catch {
                    debugPrint("fatsort", command, error)
                }
            }
            do {
                let data = try Authorization.executeWithPrivileges("\(url.path) \(drive.bsdNode) -l")
                let stdout = String(bytes: data.readDataToEndOfFile(), encoding: .utf8)!
                debugPrint("fatsort -l", stdout)
                updateFilesView(stdout)
            } catch {
                debugPrint("fatsort -l", error)
            }
            DADiskMount(daDisk, nil, DADiskMountOptions(kDADiskMountOptionDefault), Window.afterMount, nil)
        }
    }

    private static func updateFilesView(_ stdout: String) {
        // window.filesOnDrive = Window.fatsortListingToHierarchy("File system: exFAT.\n\n/\n.Spotlight-V100\n.Trashes\na\nb\n._1.mp3\n._2.mp3\n1.mp3\n2.mp3\n3.mp3\nALBUM.PIC\nM3U.LIB\nMUSIC.LIB\nUSERPL1.PL\nUSERPL2.PL\nUSERPL3.PL\n\n/.Spotlight-V100/\nStore-V2\nVolumeConfiguration.plist\n\n/.Spotlight-V100/Store-V2/\n\n/.Trashes/\n501\n._501\n\n/.Trashes/501/\n\n/a/\n._1.mp3\n._2.mp3\n1.mp3\n2.mp3\n3.mp3\nsuba\n\n/a/suba/\n3.mp3\n\n/b/\n._1.mp3\n._2.mp3\n1.mp3\n2.mp3\n3.mp3")
        window.filesOnDrive = Window.fatsortListingToHierarchy(stdout)
        window.filesView.files = window.filesOnDrive
        window.filesView.reloadData()
    }

    static let afterMount: DADiskMountCallback = { daDisk, dissenter, context in
        debugPrint("afterMount")
        window.sortButton.isEnabled = true
        window.listButton.isEnabled = true
    }

    static func fatsortListingToHierarchy(_ stdout: String) -> File {
        let h = File("/")
        if !stdout.isEmpty {
            stdout.trimmingCharacters(in: .whitespacesAndNewlines).drop { $0 != "/" }.components(separatedBy: "\n\n").forEach { block in
                let block = block.components(separatedBy: "\n")
                let blockFolder = block.first!
                let blockFolderPathComponents = URL(fileURLWithPath: blockFolder).pathComponents
                if !(blockFolderPathComponents.contains { $0.starts(with: ".") }) {
                    let hh = blockFolderPathComponents.reduce(h) { (acc, e) in
                        if e == "/" {
                            return acc
                        }
                        if let fold = (acc.files.first { $0.name == e }) {
                            return fold
                        }
                        let ff = File(e)
                        acc.files.append(ff)
                        return ff
                    }
                    hh.files.append(contentsOf: block.dropFirst().compactMap { $0.starts(with: ".") ? nil : File($0) })
                }
            }
        }
        return h
    }

    static func fatsortFlagsFromUiSettings() -> String {
        var args = [String]()
        if window.directoryButton.title != window.defaultDirectoryTitle {
            args.append("-D \(window.directoryButton.title)")
        }
        switch Order(rawValue: window.sortByDropdown.title) {
            case .directoriesFirst: args.append("-o d")
            case .filesFirst: args.append("-o f")
            case .mixed: args.append("-o a")
            case .none: break // can't happen
        }
        if args.isEmpty {
            return ""
        }
        return " " + args.joined(separator: " ")
    }

    @objc func list() {
        debugPrint("list")
        listButton.isEnabled = false
        runFatsortCommand()
    }

    @objc func sort() {
        debugPrint("sort")
        sortButton.isEnabled = false
        runFatsortCommand()
    }

    @objc func selectDirectory() {
        let openPanel = NSOpenPanel()
        openPanel.canChooseFiles = false
        openPanel.canChooseDirectories = true
        openPanel.allowsMultipleSelection = false
        openPanel.directoryURL = URL(fileURLWithPath: drive!.path)
        openPanel.beginSheetModal(for: self) { response in
            if response == .OK {
                self.directoryButton.title = String(openPanel.directoryURL!.path.dropFirst(self.drive!.path.count + 1))
                self.clearButton.isHidden = false
            }
            openPanel.close()
        }
    }

    @objc func clearDirectory() {
        clearButton.isHidden = true
        directoryButton.title = defaultDirectoryTitle
    }

    func runFatsortCommand() {
        DADiskUnmount(drive!.daDisk, DADiskUnmountOptions(kDADiskUnmountOptionDefault), Window.afterUnmount, nil)
    }
}

enum Order: String, CaseIterable {
    case directoriesFirst = "Directories first"
    case filesFirst = "Files first"
    case mixed = "Mixed"

    @available(macOS 11.0, *)
    func image() -> NSImage {
        return NSImage(systemSymbolName: imageName(), accessibilityDescription: nil)!
    }

    private func imageName() -> String {
        switch self {
            case .directoriesFirst: return "folder"
            case .filesFirst: return "doc"
            case .mixed: return "doc.on.doc"
        }
    }

    static func fromLocalizedString(_ localizedString: String) -> Order {
        switch localizedString {
            case Order.directoriesFirst.localizedString: return .directoriesFirst
            case Order.filesFirst.localizedString: return .filesFirst
            case Order.mixed.localizedString: return .mixed
            default: fatalError() // can't happen
        }
    }

    var localizedString: String {
        switch self {
            case .directoriesFirst: return NSLocalizedString("Focus selected window", comment: "")
            case .filesFirst: return NSLocalizedString("Do nothing", comment: "")
            case .mixed: return NSLocalizedString("Do nothing", comment: "")
        }
    }
}

class Drive {
    let path: String
    let name: String
    let bsdNode: String
    let daDisk: DADisk

    init(_ path: String, _ name: String, _ bsdNode: String, _ daDisk: DADisk) {
        self.path = path
        self.name = name
        self.bsdNode = bsdNode
        self.daDisk = daDisk
    }
}
