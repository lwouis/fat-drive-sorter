struct RuntimeError: Error {
    let message: String

    init(_ message: String...) {
        self.message = message.joined(separator: " ")
    }

    public var localizedDescription: String {
        message
    }
}