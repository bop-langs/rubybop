class Main
  @a=0
  @b=0
  @c=0
  @d=0

  def run()
    PPR{
      sleep(1)
      @a=1
    }
    PPR{
      sleep(1)
      @b=2
    }
    PPR{
      sleep(1)
      @c=3
    }
    PPR{
      sleep(1)
      @d=4
    }
    return self
  end

  def pl()
    puts @a
    puts @b
    puts @c
    puts @d

  end
end
Main.new().run().pl()
