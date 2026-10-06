// What the KI area uses of the QR code library, and nothing else.
//
// Written by hand, like hls.d.ts beside it: the package's own declaration
// describes a global, not a module.

export interface QrCode {
	addData(data: string): void;
	make(): void;
	getModuleCount(): number;
	isDark(row: number, col: number): boolean;
}

declare function qrcode(typeNumber: number, errorCorrectionLevel: 'L' | 'M' | 'Q' | 'H'): QrCode;

export default qrcode;
