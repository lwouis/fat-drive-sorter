class FilesView: NSOutlineView, NSOutlineViewDelegate, NSOutlineViewDataSource {
    var files: File?

    static func generate(_ files: File) -> FilesView {
        let f = FilesView()
        f.headerView = nil
        f.backgroundColor = .white
        f.usesAlternatingRowBackgroundColors = true
        f.files = files
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(rawValue: "outlineViewColumn"))
        column.isEditable = false
        column.maxWidth = 500
        f.addTableColumn(column)
        f.outlineTableColumn = column
        f.delegate = f
        f.dataSource = f
        return f
    }

    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        if let item = item as? File {
            return item.files.count
        }
        return files!.files.count
    }

    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        if let item = item as? File {
            return item.files[index]
        }
        return files!.files[index]
    }

    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        if let item = item as? File {
            return item.files.count > 0
        }
        return false
    }

    func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        let item = item as! File
        return NSTextField(labelWithString: item.name)
    }
}

class File {
    let name: String
    var files: [File]

    init(_ name: String, _ files: [File] = []) {
        self.name = name
        self.files = files
    }
}
