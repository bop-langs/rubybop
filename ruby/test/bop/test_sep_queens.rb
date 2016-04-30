# http://steamcode.blogspot.com/2010/06/n-queens-problem-in-ruby.html

# We present today a classic exercise that has been on my to-do list since the
# start of Programming Praxis.
# 
# The n-queens problem is to find all possible ways to place n queens on an n ×
# n chess board in such a way that no two queens share a row, column, or
# diagonal. The diagram at right shows one way that can be done on a standard 8
# × 8 chess board.
# 
# Your task is to write a program to find all such placements. When you are
# finished, you are welcome to read or run a suggested solution, or to post
# your own solution or discuss the exercise in the comments below.

class Board
    attr_reader :valid

    # Create a new board, initialize with with all 0 and
    # save the size of it.
    def initialize(n, rag)
        @n = n
        @iter_range = rag
        @valid = 0
        @board = Array.new(n)
        @board.map! { Array.new(n, 0) }
    end

    # Print the current board.
    def print_board
        puts "Board:"
        @board.each_index do |row|
            @board.each_index do |col|
                print "#{@board[row][col]}"
            end
            puts
        end
    end

    # Check if the row is safe by looping through each of the 
    # columns in the row.
    def safe_row(suggested_row)
        0.upto(@n-1) do |col|
            return false if @board[suggested_row][col] == 1
        end

        return true
    end

    # Check if the column is safe by looping through each of the 
    # rows in the row.
    def safe_col(suggested_col)
        0.upto(@n-1) do |row|
            return false if @board[row][suggested_col] == 1
        end

        return true
    end

    # Loop through in one diagonal direction to determine if the 
    # suggested row and column are safe.
    def safe_diag(suggested_row, suggested_col, row_mod, col_mod)
        row,col = suggested_row+row_mod, suggested_col+col_mod
        while true do

            # Break out of the loop if the row or column is off the board.
            break if (row >= @n) || (col >= @n) || (row < 0) || (col < 0)

            # If this row or column has a queen, then it's not safe.
            return false if @board[row][col] == 1
                
            # Move in the appropriate direction.
            row += row_mod
            col += col_mod
        end

        # This direction is safe.
        return true
    end

    def safe(suggested_row, suggested_col)

        # Check the rows and columns for safe.
        return false if !safe_row(suggested_row)
        return false if !safe_col(suggested_col)

        # Check the diagonals for safe.
        return false if !safe_diag(suggested_row, suggested_col, 1, 1)
        return false if !safe_diag(suggested_row, suggested_col, 1, -1)
        return false if !safe_diag(suggested_row, suggested_col, -1, 1)
        return false if !safe_diag(suggested_row, suggested_col, -1, -1)

        # Should be OK.
        return true
    end

    # Solve the n-queens problem by making a call to the recursive solve_1
    # method with 0 (the first row of the board) to start.
    def solve
        PPR do
            @iter_range.each do |col|
                @board[0][col]=1
                solve_1(1)
                @board[0][col]=0
            end
        end
    end

    # The recursive method (by row) that loops through the columns and checks
    # if the row given and the column are "safe". If they are we add a 1 to
    # the position and if the row is 0, we print it out (everthing is
    # complete). If it's safe adn we aren't at 0 we move to the next row
    # recursively.  Finally, we reset the position to 0 (blank) when we
    # return from the recursive call or from printing the board.  If it's not
    # "safe" then we just move to the next column and try that one.
    def solve_1(row)
        0.upto(@n-1) do |col|
            if safe(row, col)
                @board[row][col] = 1
                if row == (@n-1)
                    #print_board
                    @valid+=1
                else
                    solve_1(row+1)
                end
                @board[row][col] = 0
            end
        end
    end
end


# Solve the problem for 8 queens. There should be 92
# solutions.
bsize = ARGV[0].to_i
gsize = PPR.get_group_size
boards = gsize.times.map do |task|
    Board.new(bsize, (task * bsize / gsize).upto((task+1)*bsize/gsize - 1))
end
boards.each{|b| b.solve}
PPR.over
puts boards.inject(0){|sum, b| sum + b.valid}
