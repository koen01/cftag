package dngsoftware.spoolid;

public class SpoolItem {
    public int id;
    public String filamentName;   // "Vendor · Name" or just name if no vendor
    public String colorHex;       // "FF0000" or null
    public double remainingWeight; // grams, 0 if unknown
}
