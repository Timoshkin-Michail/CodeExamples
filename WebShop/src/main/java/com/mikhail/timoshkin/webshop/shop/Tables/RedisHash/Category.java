package com.mikhail.timoshkin.webshop.shop.Tables.RedisHash;

import lombok.Data;
import org.springframework.data.redis.core.RedisHash;

import java.util.ArrayList;
import java.util.List;

@RedisHash("Category")
@Data
public class Category {
    private String id; // category name
    private List<Integer> itemIds = new ArrayList<>();
}
