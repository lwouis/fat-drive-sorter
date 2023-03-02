public struct Authorization {
    static var authorizationRef: AuthorizationRef?

    public enum Error: Swift.Error {
        case create(OSStatus)
        case copyRights(OSStatus)
        case exec(OSStatus)
    }

    public static func executeWithPrivileges(_ command: String) throws -> FileHandle {
        let RTLD_DEFAULT = UnsafeMutableRawPointer(bitPattern: -2)
        let fn = dlsym(RTLD_DEFAULT, "AuthorizationExecuteWithPrivileges")
        var AuthorizationExecuteWithPrivileges: @convention(c) (
            AuthorizationRef,
            UnsafePointer<CChar>,
            AuthorizationFlags,
            UnsafePointer<UnsafePointer<CChar>?>,
            UnsafeMutablePointer<UnsafeMutablePointer<FILE>>?
        ) -> OSStatus
        AuthorizationExecuteWithPrivileges = unsafeBitCast(fn, to: type(of: AuthorizationExecuteWithPrivileges))
        if authorizationRef == nil {
            let status = AuthorizationCreate(nil, nil, [], &authorizationRef)
            if status != errAuthorizationSuccess {
                throw RuntimeError("AuthorizationCreate", String(status))
            }
        }
        var components = command.components(separatedBy: " ")
        var path = components.remove(at: 0).cString(using: .utf8)!
        let name = kAuthorizationRightExecute.cString(using: .utf8)!
        var items: AuthorizationItem = name.withUnsafeBufferPointer { nameBuf in
            path.withUnsafeBufferPointer { pathBuf in
                let pathPtr = UnsafeMutableRawPointer(mutating: pathBuf.baseAddress!)
                return AuthorizationItem(name: nameBuf.baseAddress!, valueLength: path.count, value: pathPtr, flags: 0)
            }
        }
        var rights: AuthorizationRights = withUnsafeMutablePointer(to: &items) { items in
            return AuthorizationRights(count: 1, items: items)
        }
        let status = AuthorizationCopyRights(authorizationRef!, &rights, nil, [.interactionAllowed, .extendRights], nil)
        if status != errAuthorizationSuccess {
            throw RuntimeError("AuthorizationCopyRights", String(status))
        }
        let rest = components.map { $0.cString(using: .utf8)! }
        var args = Array<UnsafePointer<CChar>?>(repeating: nil, count: rest.count + 1)
        for (idx, arg) in rest.enumerated() {
            args[idx] = UnsafePointer<CChar>?(arg)
        }
        var file = FILE()
        let fh: FileHandle = try withUnsafeMutablePointer(to: &file) { file in
            var pipe = file
            let status = AuthorizationExecuteWithPrivileges(authorizationRef!, &path, [], &args, &pipe)
            if status != errAuthorizationSuccess {
                throw RuntimeError("AuthorizationExecuteWithPrivileges", String(status))
            }
            return FileHandle(fileDescriptor: fileno(pipe), closeOnDealloc: true)
        }
        return fh
    }
}
