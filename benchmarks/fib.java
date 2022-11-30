class GFG {
  
    // Function to print the fibonacci series
    static int fib(int n)
    {
        // Base Case
        if (n == 1)
            return 1;
         if (n == 2)
            return 1;
  
  
        // Recursive call
        return fib(n - 1)
            + fib(n - 2);
    }
  
    // Driver Code
    public static void
    main(String args[])
    {
            System.out.print(fib(42) + " ");
    }
}