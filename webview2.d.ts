import {Buffer} from 'buffer';
import {EventEmitter} from 'events';

declare function MessageLoop(): void

declare function SetBrowserLocation(path: string): void

declare enum ResourceType {
    ALL = 0,
    DOCUMENT = 1,
    STYLESHEET = 2,
    IMAGE = 3,
    MEDIA = 4,
    FONT = 5,
    SCRIPT = 6,
    XML_HTTP_REQUEST = 7,
    FETCH = 8,
    TEXT_TRACK = 9,
    EVENT_SOURCE = 10,
    WEBSOCKET = 11,
    MANIFEST = 12,
    SIGNED_EXCHANGE = 13,
    PING = 14,
    CSP_VIOLATION_REPORT = 15,
    OTHER = 16
}

declare type Req = {
    url: string,
    type: ResourceType
};

declare type Res = {
    send: (data: typeof Buffer, contentType: string, statusCode?: number, statusText?: string) => void
};

declare class BrowserWindow extends EventEmitter {
    constructor(title: string)

    public openDevTools(): boolean;

    public setRequestHandler(handler: (req: Req, res: Res) => void): void;

    public addRequestFilter(urlMatch: string, resourceType: ResourceType): boolean;
}