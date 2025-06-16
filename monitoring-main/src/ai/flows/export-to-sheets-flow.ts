
'use server';
/**
 * @fileOverview A flow to export MQTT messages to a Google Sheet via a Google Apps Script web app.
 *
 * - exportDataToGoogleSheets - A function that handles the export process.
 * - ExportToSheetsInput - The input type for the exportDataToGoogleSheets function.
 * - ExportToSheetsOutput - The return type for the exportDataToGoogleSheets function.
 */

import {ai} from '@/ai/genkit';
import {z} from 'genkit'; 

// Define the schema for a single MQTT message, mirroring the MqttMessage type
const MqttMessageSchema = z.object({
  id: z.string(),
  topic: z.string(),
  payload: z.string(),
  timestamp: z.number(),
  qos: z.number(),
  retain: z.boolean(),
});

const ExportToSheetsInputSchema = z.object({
  messages: z.array(MqttMessageSchema).describe("An array of MQTT messages to export."),
  sheetIdentifier: z.string().optional().describe("An optional identifier (e.g., sheet name or ID) for the target Google Sheet. This is mainly for logging or user reference, as the Apps Script URL dictates the actual target."),
});
export type ExportToSheetsInput = z.infer<typeof ExportToSheetsInputSchema>;

const ExportToSheetsOutputSchema = z.object({
  sheetUrl: z.string().describe("The URL of the Google Sheet document being written to by the Apps Script."),
  statusMessage: z.string().describe("A message indicating the status of the export operation."),
});
export type ExportToSheetsOutput = z.infer<typeof ExportToSheetsOutputSchema>;

// The URL of your Google Apps Script web app
const APPS_SCRIPT_URL = 'https://script.google.com/macros/s/AKfycbxTSRqDkd14hrwLSKLSQPmLkLs2NI_BeRttyncm-OyhqXkoYo7zj9-_LA8DjBMRf9NF1g/exec';
// The URL of the target Google Sheet document (for user reference in toasts)
const TARGET_GOOGLE_SHEET_URL = 'https://docs.google.com/spreadsheets/d/1Ls_CIsD1zmtgkK_nzK4l1Gx0z9vpTFvSqtZ1WIJxmzg/edit';


export async function exportDataToGoogleSheets(input: ExportToSheetsInput): Promise<ExportToSheetsOutput> {
  return exportToSheetsFlow(input);
}

const exportToSheetsFlow = ai.defineFlow(
  {
    name: 'exportToSheetsFlow',
    inputSchema: ExportToSheetsInputSchema,
    outputSchema: ExportToSheetsOutputSchema,
  },
  async (input) => {
    try {
      if (!input.messages || input.messages.length === 0) {
        return {
          sheetUrl: TARGET_GOOGLE_SHEET_URL,
          statusMessage: 'No messages to export.',
        };
      }

      let successfulExports = 0;
      let failedExports = 0;

      for (const msg of input.messages) {
        const payloadToAppsScript = {
          topic: msg.topic,
          message: msg.payload, // Your Apps Script expects 'topic' and 'message'
        };

        try {
          const response = await fetch(APPS_SCRIPT_URL, {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
            },
            body: JSON.stringify(payloadToAppsScript),
            // IMPORTANT for Apps Script: redirect must be 'follow' for ContentService responses
            redirect: 'follow', 
          });

          const responseText = await response.text();

          if (response.ok && responseText.trim().toLowerCase() === 'success') {
            successfulExports++;
          } else {
            console.warn(`Apps Script call failed for message ID ${msg.id}: Status ${response.status}, Response: ${responseText}`);
            failedExports++;
          }
        } catch (error: any) {
          console.error(`Error sending message ID ${msg.id} to Apps Script:`, error.message, error.stack);
          failedExports++;
        }
      }

      let statusMessage = '';
      if (successfulExports > 0 && failedExports === 0) {
        statusMessage = `Successfully exported ${successfulExports} message(s) to Google Sheet via Apps Script.`;
      } else if (successfulExports > 0 && failedExports > 0) {
        statusMessage = `Exported ${successfulExports} message(s) successfully, but ${failedExports} message(s) failed to export via Apps Script. Check console for details.`;
      } else if (failedExports > 0) {
        statusMessage = `Failed to export ${failedExports} message(s) to Google Sheet via Apps Script. Check console for details.`;
      } else {
        statusMessage = 'No messages were processed for export (this should not happen if input messages were present).';
      }
      
      if (input.sheetIdentifier) {
          statusMessage += ` (Target identifier provided: ${input.sheetIdentifier})`;
      }

      return {
        sheetUrl: TARGET_GOOGLE_SHEET_URL,
        statusMessage: statusMessage,
      };
    } catch (flowError: any) {
      console.error('[exportToSheetsFlow] Unhandled error in flow execution:', flowError.message, flowError.stack);
      return {
        sheetUrl: TARGET_GOOGLE_SHEET_URL, // Still provide the URL for potential manual check
        statusMessage: `Critical error in export flow: ${flowError.message || 'Unknown error'}. Please check server logs.`,
      };
    }
  }
);
