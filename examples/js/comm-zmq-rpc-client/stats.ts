import now from "@fict/utils/now";
import Tracker from "@fict/utils/tracker";

export interface IResponseTimes {
  min: number;
  max: number;
}

export interface IStats {
  lastSentAt: number;
  lastReceivedAt: number;
  sentBytes: number;
  receivedBytes: number;
  responseTimes: IResponseTimes;
  requestsCompleted: number;
}

export default class Stats implements IStats {
  lastSentAt: number = 0;
  lastReceivedAt: number = 0;
  sentBytes: number = 0;
  receivedBytes: number = 0;
  responseTimes: IResponseTimes = { min: 0, max: 0 };
  requestsCompleted: number = 0;

  onSent(buf: Uint8Array): void {
    this.lastSentAt = now();
    this.sentBytes += buf.length;
  }

  onReceived(msg: Uint8Array, t?: Tracker): void {
    this.lastReceivedAt = now();
    this.receivedBytes += msg.length;

    if (t) {
      const responseTime = t.timeSpent();
      this.responseTimes = {
        min: Math.min(responseTime, this.responseTimes.min || Infinity),
        max: Math.max(responseTime, this.responseTimes.max),
      };
      this.requestsCompleted++;
    }
  }
}
