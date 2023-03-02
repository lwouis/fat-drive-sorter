import Foundation

// queues and dedicated threads to observe background events
class BackgroundWork {
    static var crashReportsQueue: DispatchQueue!

    // Thread.start() is async; we use a semaphore to ensure threads are actually ready before we continue the launch sequence
    static let threadStartSemaphore = DispatchSemaphore(value: 0)

    // swift static variables are lazy; we artificially force the threads to init
    static func start() {
        crashReportsQueue = DispatchQueue.globalConcurrent("crashReportsQueue", .utility)
    }
}

extension DispatchQueue {
    static func globalConcurrent(_ label: String, _ qos: DispatchQoS) -> DispatchQueue {
        return DispatchQueue(label: label, attributes: .concurrent, target: .global(qos: qos.qosClass))
    }
}

class BackgroundThreadWithRunLoop {
    var thread: Thread?
    var runLoop: CFRunLoop?
    var hasSentSemaphoreSignal = false

    init(_ name: String, _ qos: DispatchQoS) {
        thread = Thread {
            self.runLoop = CFRunLoopGetCurrent()
            while !self.thread!.isCancelled {
                if !self.hasSentSemaphoreSignal {
                    BackgroundWork.threadStartSemaphore.signal()
                    self.hasSentSemaphoreSignal = true
                }
                CFRunLoopRun()
                // avoid tight loop while waiting for the first runloop source to be added
                Thread.sleep(forTimeInterval: 0.1)
            }
        }
        thread!.name = name
        thread!.qualityOfService = qos.toQualityOfService()
        thread!.start()
        BackgroundWork.threadStartSemaphore.wait()
    }
}

extension DispatchQoS {
    func toQualityOfService() -> QualityOfService {
        switch self {
            case .userInteractive: return .userInteractive
            case .userInitiated: return .userInitiated
            case .utility: return .utility
            case .background: return .background
            default: return .default
        }
    }
}
